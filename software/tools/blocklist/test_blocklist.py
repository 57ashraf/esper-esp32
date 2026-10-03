#!/usr/bin/env python3

from __future__ import annotations

import struct
import unittest

from blocklist_format import (
    HEADER_SIZE,
    HEADER_STRUCT,
    IndexedBlocklist,
    Rule,
    build_binary,
    canonicalize_domain,
    classify_pattern,
    parse_rules,
    rule_matches,
)


class BlocklistTests(unittest.TestCase):
    def test_generator_reproducibility(self) -> None:
        rules = parse_rules(["ADS.example", "*.wild.example", "ads.example", "track?.foo"])
        self.assertEqual(build_binary(rules)[0], build_binary(list(reversed(rules)))[0])

    def test_canonical_case_and_trailing_dot(self) -> None:
        self.assertEqual(canonicalize_domain("  Ad.Example.COM... "), "ad.example.com")
        self.assertIsNone(canonicalize_domain("a..example.com"))
        self.assertIsNone(canonicalize_domain("bad space.example"))

    def test_suffix_is_label_aware(self) -> None:
        rule = classify_pattern("Example.COM")
        assert rule is not None
        self.assertTrue(rule_matches("EXAMPLE.com.", rule))
        self.assertTrue(rule_matches("a.example.com", rule))
        self.assertFalse(rule_matches("badexample.com", rule))

    def test_wildcard_subdomain_excludes_apex(self) -> None:
        rule = classify_pattern("*.Example.com.")
        assert rule is not None
        self.assertEqual(rule.kind, "wildcard_subdomain")
        self.assertFalse(rule_matches("example.com", rule))
        self.assertTrue(rule_matches("one.example.com", rule))
        self.assertTrue(rule_matches("deep.one.example.com", rule))
        self.assertFalse(rule_matches("badexample.com", rule))

    def test_generic_wildcards_are_label_aware(self) -> None:
        rule = classify_pattern("*.ad.*")
        assert rule is not None
        self.assertTrue(rule_matches("foo.ad.example", rule))
        self.assertTrue(rule_matches("foo.ad.example.co.uk", rule))
        self.assertFalse(rule_matches("foo.notad.example", rule))

        label_rule = classify_pattern("advertising*")
        assert label_rule is not None
        self.assertTrue(rule_matches("advertising.cdn.example", label_rule))
        self.assertTrue(rule_matches("cdn.advertising.example", label_rule))
        self.assertFalse(rule_matches("ad.ver-tising.example", label_rule))

    def test_parser_merges_adblock_and_hosts_rules(self) -> None:
        rules = parse_rules(
            [
                "! comment",
                "||Example.COM^",
                "*.example.com",
                "0.0.0.0 Mixed.Example.NET # comment",
                "ads.*",
            ]
        )
        kinds = {(rule.pattern, rule.kind) for rule in rules}
        self.assertIn(("example.com", "suffix_and_wildcard_subdomain"), kinds)
        self.assertIn(("mixed.example.net", "suffix"), kinds)
        self.assertIn(("ads.*", "wildcard"), kinds)

    def test_collision_is_verified_by_exact_name(self) -> None:
        fake_hash = lambda _name: 7
        image, _stats = build_binary(
            [Rule("alpha.example", "suffix"), Rule("beta.example", "suffix")],
            hash_function=fake_hash,
        )
        indexed = IndexedBlocklist(image, hash_function=fake_hash)
        self.assertTrue(indexed.lookup("ALPHA.EXAMPLE."))
        self.assertTrue(indexed.lookup("child.alpha.example"))
        self.assertTrue(indexed.lookup("beta.example"))
        self.assertFalse(indexed.lookup("gamma.example"))

    def test_corrupt_header_is_rejected(self) -> None:
        image, _stats = build_binary([Rule("example.com", "suffix")])
        corrupt = bytearray(image)
        struct.pack_into("<I", corrupt, 32, len(image) + 1)
        with self.assertRaises(ValueError):
            IndexedBlocklist(bytes(corrupt))

    def test_binary_lookup_semantics(self) -> None:
        image, _stats = build_binary(
            [
                Rule("example.com", "suffix"),
                Rule("wild.example", "wildcard_subdomain"),
                Rule("*.ad.*", "wildcard"),
            ]
        )
        indexed = IndexedBlocklist(image)
        self.assertTrue(indexed.lookup("EXAMPLE.COM."))
        self.assertTrue(indexed.lookup("x.example.com"))
        self.assertFalse(indexed.lookup("badexample.com"))
        self.assertFalse(indexed.lookup("wild.example"))
        self.assertTrue(indexed.lookup("x.wild.example"))
        self.assertTrue(indexed.lookup("foo.ad.cdn.example"))
        self.assertFalse(indexed.lookup("foo.notad.cdn.example"))


if __name__ == "__main__":
    unittest.main()

