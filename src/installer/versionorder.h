// SPDX-License-Identifier: MIT
#pragma once
#include <regex>
#include <string>
#include <vector>
namespace InstallerUI {
inline std::vector<std::string> versionParts(const std::string &value) {
    std::vector<std::string> result;
    size_t begin=0;
    do {const auto end=value.find('.',begin);result.push_back(value.substr(begin,end-begin));
        if(end==std::string::npos)break;begin=end+1;} while(true);
    return result;
}
inline bool numericPart(const std::string &value) {
    return !value.empty() && value.find_first_not_of("0123456789")==std::string::npos;
}
inline int numberOrder(std::string a,std::string b) {
    const auto normalize=[](std::string &v) {auto n=v.find_first_not_of('0');v=n==std::string::npos?"0":v.substr(n);};
    normalize(a);normalize(b);
    if(a.size()!=b.size())return a.size()>b.size()?1:-1;
    return a==b?0:a>b?1:-1;
}
// Compare raw versions, not the shortened internal-build display labels.
// Unknown metadata cannot establish a downgrade; retain the repair path.
inline bool newerVersion(const std::string &installed,const std::string &bundled,
                         const std::string &installedBuild={},const std::string &bundledBuild={}) {
    static const std::regex semantic(R"(^\s*(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?(?:\+([0-9A-Za-z.-]+))?\s*$)");
    std::smatch a,b;
    if(!std::regex_match(installed,a,semantic) || !std::regex_match(bundled,b,semantic))return false;
    for(int i=1;i<=3;++i)if(const int order=numberOrder(a[i],b[i]))return order>0;
    const auto preA=a[4].str(),preB=b[4].str();
    if(preA!=preB) {
        if(preA.empty() || preB.empty())return preA.empty();
        const auto pa=versionParts(preA),pb=versionParts(preB);
        for(size_t i=0;i<pa.size() && i<pb.size();++i) {
            const bool na=numericPart(pa[i]),nb=numericPart(pb[i]);
            if(na!=nb)return !na;
            const int order=na?numberOrder(pa[i],pb[i]):pa[i]==pb[i]?0:pa[i]>pb[i]?1:-1;
            if(order)return order>0;
        }
        return pa.size()>pb.size();
    }
    const auto buildKey=[](const std::string &metadata,const std::string &build) {
        std::smatch m;
        static const std::regex internal(R"(^internal\.(\d{8})\.(\d+)$)");
        static const std::regex dated(R"(^\s*(\d{4})\.(\d{2})\.(\d{2})\.(\d+)\s*$)");
        if(std::regex_match(metadata,m,internal))return std::vector<std::string>{m[1],m[2]};
        if(std::regex_match(build,m,dated))return std::vector<std::string>{m[1].str()+m[2].str()+m[3].str(),m[4]};
        return std::vector<std::string>{};
    };
    const auto ba=buildKey(a[5],installedBuild),bb=buildKey(b[5],bundledBuild);
    // Internal builds are iterations after their unchanged base release VERSION.
    if(ba.empty()!=bb.empty())return !ba.empty();
    for(size_t i=0;i<ba.size();++i)if(const int order=numberOrder(ba[i],bb[i]))return order>0;
    return false;
}
}
