"""Host-side format and matcher for Esper's indexed LittleFS blocklist.

The firmware deliberately keeps the same small, dependency-free format.  The
records are sorted by a 64-bit FNV-1a hash, but every matching record is
verified against its stored canonical name.  A hash match is therefore only a
candidate and never a blocking decision by itself.
"""

from __future__ import annotations

from dataclasses import dataclass
import re
import struct
from typing import Callable, Iterable, List, Optional, Sequence, Tuple


MAGIC = b"ESBL"
VERSION = 1
HEADER_SIZE = 40
RECORD_SIZE = 16
WILDCARD_HEADER_SIZE = 4
MAX_LABEL_LENGTH = 63
MAX_DOMAIN_LENGTH = 253

RULE_SUFFIX = 0x01
RULE_WILDCARD_SUBDOMAIN = 0x02
RULE_GENERIC_WILDCARD = 0x04

HEADER_STRUCT = struct.Struct("<4sHHIIIIIIII")
RECORD_STRUCT = struct.Struct("<QIHBx")
WILDCARD_STRUCT = struct.Struct("<BBH")


@dataclass(frozen=True)
class Rule:
    """A canonical rule before it is merged into the binary index."""

    pattern: str
    kind: str


@dataclass(frozen=True)
class BinaryMetadata:
    entry_count: int
    record_offset: int
    string_offset: int
    string_size: int
    wildcard_offset: int
    wildcard_size: int
    file_size: int
    wildcard_count: int


def canonicalize_domain(value: str, *, allow_wildcards: bool = False) -> Optional[str]:
    """Return a lower-case, label-validated DNS name or ``None``.

    DNS names are case-insensitive.  A final root dot is presentation syntax,
    so it is removed before checking label boundaries.  The matcher accepts
    ASCII underscore labels because service and HTTPS records can use them;
    non-ASCII input is left to a future IDNA-aware layer rather than being
    silently transformed.
    """

    if not isinstance(value, str):
        return None
    value = value.strip()
    if not value:
        return None
    value = value.rstrip(".")
    if not value or len(value) > MAX_DOMAIN_LENGTH:
        return None

    labels = value.split(".")
    if any(not label or len(label) > MAX_LABEL_LENGTH for label in labels):
        return None

    normalized: List[str] = []
    for label in labels:
        out: List[str] = []
        for char in label:
            code = ord(char)
            is_ascii_alnum = (
                0x30 <= code <= 0x39
                or 0x41 <= code <= 0x5A
                or 0x61 <= code <= 0x7A
            )
            if is_ascii_alnum or char in "-_":
                out.append(char.lower())
            elif allow_wildcards and char in "*?":
                out.append(char)
            else:
                return None
        normalized.append("".join(out))

    result = ".".join(normalized)
    return result if result else None


def _wildcard_subdomain_base(pattern: str) -> Optional[str]:
    if not pattern.startswith("*."):
        return None
    base = pattern[2:]
    if "*" in base or "?" in base:
        return None
    base = canonicalize_domain(base)
    if base is None or "." not in base:
        return None
    return base


def classify_pattern(pattern: str) -> Optional[Rule]:
    canonical = canonicalize_domain(pattern, allow_wildcards=True)
    if canonical is None:
        return None
    base = _wildcard_subdomain_base(canonical)
    if base is not None:
        return Rule(base, "wildcard_subdomain")
    if "*" in canonical or "?" in canonical:
        return Rule(canonical, "wildcard")
    return Rule(canonical, "suffix")


_ADBLOCK_DELIMITER = re.compile(r"[\^$|/]")


def parse_line(line: str) -> List[Rule]:
    """Parse common Adblock, hosts-file, and plain-domain syntax."""

    text = line.strip()
    if not text or text.startswith(("!", "#", "[", "@@")):
        return []

    # HaGeZi's selected feed is Adblock syntax.  A domain anchored by || is a
    # DNS suffix rule; cosmetic/path options are intentionally ignored.
    if text.startswith("||"):
        domain = _ADBLOCK_DELIMITER.split(text[2:], maxsplit=1)[0]
        rule = classify_pattern(domain)
        return [rule] if rule is not None else []

    fields = text.split()
    if fields and fields[0] in {"0.0.0.0", "127.0.0.1", "::", "::1"}:
        result: List[Rule] = []
        for field in fields[1:]:
            if field.startswith(("#", "!")):
                break
            rule = classify_pattern(field)
            if rule is not None:
                result.append(rule)
        return result

    # URL rules, regular expressions, and other browser-only filters are not
    # valid DNS names and are safely skipped.
    if text.startswith(("|", "/")) or "://" in text:
        return []

    rule = classify_pattern(fields[0])
    return [rule] if rule is not None else []


def parse_rules(lines: Iterable[str]) -> List[Rule]:
    merged_suffix: dict[str, int] = {}
    generic: set[str] = set()
    for line in lines:
        for rule in parse_line(line):
            if rule.kind == "suffix":
                merged_suffix[rule.pattern] = (
                    merged_suffix.get(rule.pattern, 0) | RULE_SUFFIX
                )
            elif rule.kind == "wildcard_subdomain":
                merged_suffix[rule.pattern] = (
                    merged_suffix.get(rule.pattern, 0) | RULE_WILDCARD_SUBDOMAIN
                )
            else:
                generic.add(rule.pattern)

    # Preserve flags by using a synthetic Rule spelling for the builder's
    # merger.  The public result stays easy to inspect in tests and reports.
    rules: List[Rule] = []
    for name, flags in sorted(merged_suffix.items()):
        if flags == RULE_SUFFIX:
            rules.append(Rule(name, "suffix"))
        elif flags == RULE_WILDCARD_SUBDOMAIN:
            rules.append(Rule(name, "wildcard_subdomain"))
        else:
            rules.append(Rule(name, "suffix_and_wildcard_subdomain"))
    rules.extend(Rule(pattern, "wildcard") for pattern in sorted(generic))
    return rules


def fnv1a64(value: str) -> int:
    result = 0xCBF29CE484222325
    for byte in value.encode("ascii"):
        result ^= byte
        result = (result * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return result


def _merged_entries(rules: Sequence[Rule]) -> Tuple[List[Tuple[str, int]], List[str]]:
    suffix: dict[str, int] = {}
    generic: set[str] = set()
    for rule in rules:
        if rule.kind == "suffix":
            suffix[rule.pattern] = suffix.get(rule.pattern, 0) | RULE_SUFFIX
        elif rule.kind == "wildcard_subdomain":
            suffix[rule.pattern] = (
                suffix.get(rule.pattern, 0) | RULE_WILDCARD_SUBDOMAIN
            )
        elif rule.kind == "suffix_and_wildcard_subdomain":
            suffix[rule.pattern] = (
                suffix.get(rule.pattern, 0)
                | RULE_SUFFIX
                | RULE_WILDCARD_SUBDOMAIN
            )
        elif rule.kind == "wildcard":
            generic.add(rule.pattern)
        else:
            raise ValueError(f"unknown rule kind: {rule.kind}")
    return sorted(suffix.items()), sorted(generic)


def build_binary(
    rules: Sequence[Rule],
    *,
    hash_function: Callable[[str], int] = fnv1a64,
) -> Tuple[bytes, dict]:
    """Build an ESBL v1 image and return it with generation statistics."""

    suffix, wildcards = _merged_entries(rules)
    strings = bytearray()
    string_offsets: dict[str, Tuple[int, int]] = {}
    for name, _flags in suffix:
        encoded = name.encode("ascii")
        string_offsets[name] = (len(strings), len(encoded))
        strings.extend(encoded)

    indexed = [
        (hash_function(name) & 0xFFFFFFFFFFFFFFFF, name, flags)
        for name, flags in suffix
    ]
    indexed.sort(key=lambda item: (item[0], item[1]))

    record_offset = HEADER_SIZE
    string_offset = record_offset + RECORD_SIZE * len(indexed)
    wildcard_bytes = bytearray()
    for pattern in wildcards:
        encoded = pattern.encode("ascii")
        if len(encoded) > 0xFFFF:
            raise ValueError("wildcard pattern is too long")
        wildcard_bytes.extend(
            WILDCARD_STRUCT.pack(RULE_GENERIC_WILDCARD, 0, len(encoded))
        )
        wildcard_bytes.extend(encoded)
    wildcard_offset = string_offset + len(strings)
    file_size = wildcard_offset + len(wildcard_bytes)

    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION,
        HEADER_SIZE,
        len(indexed),
        record_offset,
        string_offset,
        len(strings),
        wildcard_offset,
        len(wildcard_bytes),
        file_size,
        len(wildcards),
    )
    records = bytearray()
    for digest, name, flags in indexed:
        string_off, length = string_offsets[name]
        records.extend(RECORD_STRUCT.pack(digest, string_off, length, flags))

    collision_groups = 0
    collision_records = 0
    cursor = 0
    while cursor < len(indexed):
        end = cursor + 1
        while end < len(indexed) and indexed[end][0] == indexed[cursor][0]:
            end += 1
        if end - cursor > 1:
            collision_groups += 1
            collision_records += end - cursor
        cursor = end

    image = bytes(header + records + strings + wildcard_bytes)
    stats = {
        "entry_count": len(indexed),
        "wildcard_count": len(wildcards),
        "file_size": len(image),
        "string_size": len(strings),
        "record_size": len(records),
        "collision_groups": collision_groups,
        "collision_records": collision_records,
        "bytes_per_suffix_entry": (
            len(image) / len(indexed) if indexed else 0.0
        ),
    }
    return image, stats


def read_metadata(data: bytes, *, hash_function: Callable[[str], int] = fnv1a64) -> BinaryMetadata:
    if len(data) < HEADER_SIZE:
        raise ValueError("blocklist is shorter than its header")
    (
        magic,
        version,
        header_size,
        entry_count,
        record_offset,
        string_offset,
        string_size,
        wildcard_offset,
        wildcard_size,
        file_size,
        wildcard_count,
    ) = HEADER_STRUCT.unpack_from(data)
    if magic != MAGIC or version != VERSION or header_size != HEADER_SIZE:
        raise ValueError("unsupported ESBL header")
    if file_size != len(data):
        raise ValueError("file size does not match ESBL header")
    record_end = record_offset + entry_count * RECORD_SIZE
    string_end = string_offset + string_size
    wildcard_end = wildcard_offset + wildcard_size
    if (
        record_offset != HEADER_SIZE
        or record_end != string_offset
        or string_end != wildcard_offset
        or wildcard_end != file_size
        or record_end > len(data)
    ):
        raise ValueError("ESBL offsets are inconsistent")

    previous: Optional[Tuple[int, str]] = None
    for index in range(entry_count):
        digest, string_off, length, flags = RECORD_STRUCT.unpack_from(
            data, record_offset + index * RECORD_SIZE
        )
        if not flags & (RULE_SUFFIX | RULE_WILDCARD_SUBDOMAIN) or flags & ~3:
            raise ValueError("suffix record has no supported rule flag")
        if not 1 <= length <= 253 or data[record_offset + index * RECORD_SIZE + 15] != 0:
            raise ValueError("invalid suffix length/reserved byte")
        if string_off + length > string_size:
            raise ValueError("suffix record points outside string table")
        name = data[string_offset + string_off : string_offset + string_off + length]
        text = name.decode("ascii")
        if canonicalize_domain(text) != text or hash_function(text) & 0xffffffffffffffff != digest:
            raise ValueError("noncanonical name or hash mismatch")
        if previous is not None and (digest, name.decode("ascii")) < previous:
            raise ValueError("suffix records are not sorted")
        previous = (digest, name.decode("ascii"))

    cursor = wildcard_offset
    parsed = 0
    while cursor < wildcard_end:
        if cursor + WILDCARD_HEADER_SIZE > wildcard_end:
            raise ValueError("truncated wildcard record")
        flags, _reserved, length = WILDCARD_STRUCT.unpack_from(data, cursor)
        if flags != RULE_GENERIC_WILDCARD or _reserved or not 1 <= length <= 253:
            raise ValueError("unsupported wildcard record flag")
        cursor += WILDCARD_HEADER_SIZE
        if cursor + length > wildcard_end:
            raise ValueError("wildcard record points outside section")
        try:
            wildcard_text = data[cursor : cursor + length].decode("ascii")
        except UnicodeDecodeError as exc:
            raise ValueError("wildcard pattern is not ASCII") from exc
        if canonicalize_domain(wildcard_text, allow_wildcards=True) != wildcard_text or not any(c in wildcard_text for c in "*?"):
            raise ValueError("invalid wildcard pattern")
        cursor += length
        parsed += 1
    if parsed != wildcard_count:
        raise ValueError("wildcard count does not match section")

    return BinaryMetadata(
        entry_count,
        record_offset,
        string_offset,
        string_size,
        wildcard_offset,
        wildcard_size,
        file_size,
        wildcard_count,
    )


def _label_glob(pattern: str, value: str) -> bool:
    p = s = 0
    star = -1
    star_value = 0
    while s < len(value):
        if p < len(pattern) and (pattern[p] == "?" or pattern[p] == value[s]):
            p += 1
            s += 1
        elif p < len(pattern) and pattern[p] == "*":
            star = p
            p += 1
            star_value = s
        elif star >= 0:
            p = star + 1
            star_value += 1
            s = star_value
        else:
            return False
    while p < len(pattern) and pattern[p] == "*":
        p += 1
    return p == len(pattern)


def wildcard_match(pattern: str, domain: str) -> bool:
    pattern_labels = pattern.split(".")
    domain_labels = domain.split(".")
    if len(pattern_labels) == 1:
        return any(_label_glob(pattern_labels[0], label) for label in domain_labels)
    if len(pattern_labels) > len(domain_labels):
        return False
    for start in range(len(domain_labels) - len(pattern_labels) + 1):
        if all(
            _label_glob(pattern_label, domain_labels[start + offset])
            for offset, pattern_label in enumerate(pattern_labels)
        ):
            return True
    return False


class IndexedBlocklist:
    """A host implementation of the firmware's flash lookup algorithm."""

    def __init__(
        self,
        data: bytes,
        *,
        hash_function: Callable[[str], int] = fnv1a64,
    ) -> None:
        self.data = data
        self.meta = read_metadata(data, hash_function=hash_function)
        self.hash_function = hash_function

    def _record(self, index: int) -> Tuple[int, int, int, int]:
        return RECORD_STRUCT.unpack_from(
            self.data, self.meta.record_offset + index * RECORD_SIZE
        )

    def _has_suffix_rule(self, candidate: str, descendant: bool) -> bool:
        target = self.hash_function(candidate) & 0xFFFFFFFFFFFFFFFF
        low, high = 0, self.meta.entry_count
        while low < high:
            middle = (low + high) // 2
            digest = self._record(middle)[0]
            if digest < target:
                low = middle + 1
            else:
                high = middle
        while low < self.meta.entry_count:
            digest, string_off, length, flags = self._record(low)
            if digest != target:
                break
            stored = self.data[
                self.meta.string_offset + string_off :
                self.meta.string_offset + string_off + length
            ]
            if stored == candidate.encode("ascii"):
                if flags & RULE_SUFFIX:
                    return True
                if descendant and flags & RULE_WILDCARD_SUBDOMAIN:
                    return True
            low += 1
        return False

    def _wildcard_patterns(self) -> Iterable[str]:
        cursor = self.meta.wildcard_offset
        end = cursor + self.meta.wildcard_size
        for _ in range(self.meta.wildcard_count):
            flags, _reserved, length = WILDCARD_STRUCT.unpack_from(self.data, cursor)
            if flags != RULE_GENERIC_WILDCARD:
                return
            cursor += WILDCARD_HEADER_SIZE
            yield self.data[cursor : cursor + length].decode("ascii")
            cursor += length

    def lookup(self, domain: str) -> bool:
        canonical = canonicalize_domain(domain)
        if canonical is None:
            return False
        labels = canonical.split(".")
        for start in range(len(labels)):
            candidate = ".".join(labels[start:])
            if self._has_suffix_rule(candidate, start > 0):
                return True
        return any(wildcard_match(pattern, canonical) for pattern in self._wildcard_patterns())


def rule_matches(domain: str, rule: Rule) -> bool:
    canonical = canonicalize_domain(domain)
    if canonical is None:
        return False
    if rule.kind == "suffix":
        return canonical == rule.pattern or canonical.endswith("." + rule.pattern)
    if rule.kind == "wildcard_subdomain":
        return canonical.endswith("." + rule.pattern)
    if rule.kind == "suffix_and_wildcard_subdomain":
        return canonical == rule.pattern or canonical.endswith("." + rule.pattern)
    return wildcard_match(rule.pattern, canonical)
