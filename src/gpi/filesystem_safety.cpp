#include "gpi/filesystem_safety.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace genomic::gpi {
namespace {
constexpr std::array<std::uint32_t,64> k={
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
std::uint32_t rotr(std::uint32_t v,unsigned n){return (v>>n)|(v<<(32U-n));}
class Sha256 {
public:
    void add(const unsigned char* data,std::size_t count){
        length_+=static_cast<std::uint64_t>(count)*8U;
        while(count>0){const auto take=std::min(count,block_.size()-used_);std::copy_n(data,take,block_.data()+used_);used_+=take;data+=take;count-=take;if(used_==block_.size()){compress();used_=0;}}
    }
    std::string finish(){
        block_[used_++]=0x80U;
        if(used_>56){std::fill(block_.begin()+static_cast<std::ptrdiff_t>(used_),block_.end(),0);compress();used_=0;}
        std::fill(block_.begin()+static_cast<std::ptrdiff_t>(used_),block_.begin()+56,0);
        for(unsigned i=0;i<8;++i) {
            block_[63-i]=static_cast<unsigned char>(length_>>(i*8U));
        }
        compress();
        std::ostringstream out;out<<std::hex<<std::setfill('0');for(const auto v:h_)out<<std::setw(8)<<v;return out.str();
    }
private:
    void compress(){
        std::array<std::uint32_t,64>w{};for(std::size_t i=0;i<16;++i)w[i]=(static_cast<std::uint32_t>(block_[i*4])<<24U)|(static_cast<std::uint32_t>(block_[i*4+1])<<16U)|(static_cast<std::uint32_t>(block_[i*4+2])<<8U)|block_[i*4+3];
        for(std::size_t i=16;i<64;++i){const auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);const auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
        auto a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],hh=h_[7];
        for(std::size_t i=0;i<64;++i){const auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25);const auto ch=(e&f)^((~e)&g);const auto t1=hh+s1+ch+k[i]+w[i];const auto s0=rotr(a,2)^rotr(a,13)^rotr(a,22);const auto maj=(a&b)^(a&c)^(b&c);const auto t2=s0+maj;hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=hh;
    }
    std::array<std::uint32_t,8>h_{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    std::array<unsigned char,64>block_{};std::size_t used_{0};std::uint64_t length_{0};
};
bool within(const std::filesystem::path& child,const std::filesystem::path& root){const auto rel=child.lexically_relative(root);return !rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..";}
bool reparse(const std::filesystem::path& path){
#ifdef _WIN32
    const auto flags=GetFileAttributesW(path.c_str());return flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
    return std::filesystem::is_symlink(std::filesystem::symlink_status(path));
#endif
}
std::string fileHash(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot hash file: "+path.string());Sha256 hash;std::array<unsigned char,65536>buffer{};while(in){in.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(buffer.size()));const auto got=in.gcount();if(got>0)hash.add(buffer.data(),static_cast<std::size_t>(got));}if(!in.eof())throw std::runtime_error("Cannot read file while hashing: "+path.string());return hash.finish();}
}

DirectoryManifest createSha256DirectoryManifest(const std::filesystem::path& root,const ImportSafetyLimits& limits){
    if(!std::filesystem::is_directory(root))throw std::invalid_argument("Manifest root must be an existing directory");
    const auto canonicalRoot=std::filesystem::canonical(root);if(reparse(canonicalRoot))throw std::invalid_argument("Protected project root must not be a symbolic link or junction");
    DirectoryManifest result;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(canonicalRoot,std::filesystem::directory_options::none)){
        if(reparse(entry.path())||std::filesystem::is_symlink(entry.symlink_status()))throw std::invalid_argument("Foreign project contains a symbolic link or junction: "+entry.path().string());
        if(!entry.is_regular_file())continue;
        if(result.files.size()>=limits.maximumFiles)throw std::invalid_argument("Foreign project exceeds file-count safety limit");
        const auto size=entry.file_size();if(size>limits.maximumFileBytes||result.totalBytes>limits.maximumTotalBytes-size)throw std::invalid_argument("Foreign project exceeds byte safety limit");
        result.totalBytes+=size;result.files.push_back({std::filesystem::relative(entry.path(),canonicalRoot).generic_string(),size,fileHash(entry.path())});
    }
    std::sort(result.files.begin(),result.files.end(),[](const auto& a,const auto& b){return a.relativePath<b.relativePath;});Sha256 aggregate;
    for(const auto& item:result.files){const auto line=item.relativePath+"\0"+std::to_string(item.size)+"\0"+item.sha256+"\n";aggregate.add(reinterpret_cast<const unsigned char*>(line.data()),line.size());}result.sha256=aggregate.finish();return result;
}

void validateIsolatedWorkspace(const std::filesystem::path& protectedRoot,const std::filesystem::path& isolatedWorkspace){
    if(protectedRoot.empty()||isolatedWorkspace.empty())throw std::invalid_argument("Protected root and isolated workspace are required");
    const auto original=std::filesystem::canonical(protectedRoot);auto parent=isolatedWorkspace.parent_path();if(parent.empty())parent=std::filesystem::current_path();
    const auto workspace=std::filesystem::weakly_canonical(parent)/isolatedWorkspace.filename();
    if(workspace==original||within(workspace,original)||within(original,workspace))throw std::invalid_argument("Isolated workspace must be outside the protected original project tree");
    for(auto cursor=workspace;!cursor.empty();cursor=cursor.parent_path()){if(std::filesystem::exists(cursor)&&reparse(cursor))throw std::invalid_argument("Isolated workspace path must not traverse a symbolic link or junction");if(cursor==cursor.root_path())break;}
}
} // namespace genomic::gpi
