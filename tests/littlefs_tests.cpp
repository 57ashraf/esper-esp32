// Actual vendored firmware littlefs core on a bounded host RAM flash model.
#include "../software/firmware/components/littlefs/src/littlefs/lfs.h"
#include <vector>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
struct Flash { std::vector<unsigned char> bytes=std::vector<unsigned char>(1728*1024,255); bool fail_sync=false; };
static int read_block(const lfs_config* c,lfs_block_t b,lfs_off_t o,void* p,lfs_size_t n) {
    auto* f=static_cast<Flash*>(c->context); size_t at=size_t(b)*c->block_size+o;
    if(at+n>f->bytes.size())return LFS_ERR_IO; memcpy(p,f->bytes.data()+at,n);return 0;
}
static int program(const lfs_config* c,lfs_block_t b,lfs_off_t o,const void* p,lfs_size_t n) {
    auto* f=static_cast<Flash*>(c->context); size_t at=size_t(b)*c->block_size+o;
    if(at+n>f->bytes.size())return LFS_ERR_IO;
    auto* data=static_cast<const unsigned char*>(p);
    for(size_t i=0;i<n;++i) {if((f->bytes[at+i]&data[i])!=data[i])return LFS_ERR_IO;f->bytes[at+i]=data[i];}
    return 0;
}
static int erase(const lfs_config* c,lfs_block_t b) {
    auto* f=static_cast<Flash*>(c->context);size_t at=size_t(b)*c->block_size;
    if(at+c->block_size>f->bytes.size())return LFS_ERR_IO;
    memset(f->bytes.data()+at,255,c->block_size);return 0;
}
static int sync_flash(const lfs_config* c) {return static_cast<Flash*>(c->context)->fail_sync?LFS_ERR_IO:0;}
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x))throw std::runtime_error(#x);}while(0)
static std::string read_file(lfs_t& fs,const char* path) {
    lfs_file_t f={};CHECK(lfs_file_open(&fs,&f,path,LFS_O_RDONLY)==0);
    auto size=lfs_file_size(&fs,&f); CHECK(size>=0);
    std::string text(size,'\0');CHECK(lfs_file_read(&fs,&f,&text[0],size)==size);
    CHECK(lfs_file_close(&fs,&f)==0);return text;
}
static void write_file(lfs_t& fs,const char* path,const std::string& value) {
    lfs_file_t f={};CHECK(lfs_file_open(&fs,&f,path,LFS_O_CREAT|LFS_O_TRUNC|LFS_O_WRONLY)==0);
    CHECK(lfs_file_write(&fs,&f,value.data(),value.size())==static_cast<int>(value.size()));
    CHECK(lfs_file_sync(&fs,&f)==0);CHECK(lfs_file_close(&fs,&f)==0);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("Provide synthetic ESBL path");
        std::ifstream input(argv[1],std::ios::binary);std::string image{std::istreambuf_iterator<char>(input),{}};
        CHECK(image.size()==1225040);
        Flash flash; lfs_config c={}; c.context=&flash;c.read=read_block;c.prog=program;c.erase=erase;c.sync=sync_flash;
        c.read_size=128;c.prog_size=128;c.block_size=4096;c.block_count=432;c.block_cycles=512;c.cache_size=512;c.lookahead_size=128;
        lfs_t fs={};CHECK(lfs_format(&fs,&c)==0);CHECK(lfs_mount(&fs,&c)==0);
        CHECK(lfs_mkdir(&fs,"/ota_0")==0);
        write_file(fs,"/ota_0/blocklist.bin",image);
        const std::string original="original settings fixture";
        write_file(fs,"/ota_0/settings.json",original);
        write_file(fs,"/ota_0/settings.json.tmp","uncommitted fixture");
        // A failed temp sync must not touch the existing valid destination.
        lfs_file_t temp={};CHECK(lfs_file_open(&fs,&temp,"/ota_0/settings.json.tmp",LFS_O_WRONLY)==0);
        CHECK(lfs_file_write(&fs,&temp,"new",3)==3);
        flash.fail_sync=true;CHECK(lfs_file_sync(&fs,&temp)==LFS_ERR_IO);flash.fail_sync=false;
        lfs_file_close(&fs,&temp);CHECK(read_file(fs,"/ota_0/settings.json")==original);
        write_file(fs,"/ota_0/settings.json.tmp","committed fixture");
        CHECK(lfs_rename(&fs,"/ota_0/settings.json.tmp","/ota_0/settings.json")==0);
        CHECK(read_file(fs,"/ota_0/settings.json")=="committed fixture");
        CHECK(lfs_unmount(&fs)==0);CHECK(lfs_mount(&fs,&c)==0);
        CHECK(read_file(fs,"/ota_0/settings.json")=="committed fixture");
        CHECK(read_file(fs,"/ota_0/blocklist.bin")==image);
        auto blocks=lfs_fs_size(&fs);CHECK(blocks>0 && blocks<432);
        std::cout<<"LITTLEFS CORE PASS: "<<checks<<" checks; disk=2.0; partition=1769472; used_blocks="
                 <<blocks<<"; allocated_bytes="<<blocks*4096<<"; free_blocks="<<432-blocks
                 <<"; modeled temp-sync fault and atomic rename/remount (not physical power-cut validation)\n";
        CHECK(lfs_unmount(&fs)==0);
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
