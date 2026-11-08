#include "session_log.hpp"
#include <iostream>
#include <cstdlib>
#include <iomanip>
void require(bool b,const char* why){if(!b){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    auto cwd=std::filesystem::current_path();auto root=cwd/("log-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));auto dir=root/"logs";
    mc::SessionLog log({128,384,3});log.setHeader("columns\n");log<<"disabled";log.flush();require(!log.enabled()&&!std::filesystem::exists(root),"default off does not create files or directories");
    require(log.start(dir),"explicit start creates recording");log<<std::fixed<<std::setprecision(2)<<3.14159<<'\n';log.flush();auto first=log.currentPath();log.stop();auto size=std::filesystem::file_size(first);log<<"must not be recorded\n";log.flush();require(std::filesystem::file_size(first)==size,"uncheck closes file and suppresses subsequent writes");
    {std::ifstream in(first);std::string text((std::istreambuf_iterator<char>(in)),{});require(text.starts_with("columns\n")&&text.find("3.14")!=std::string::npos&&text.find("must not") ==std::string::npos,"formatting works and disabled data is absent");}
    {std::ofstream unrelated(dir/"keep.txt");unrelated<<"user file";}
    require(log.start(dir),"recording can restart");for(int n=0;n<70;++n){log<<std::string(70,char('a'+n%20))<<'\n';log.flush();require(log.enabled(),"rotation stays enabled");}
    log.stop();size_t count=0;uintmax_t total=0;for(auto& file:std::filesystem::directory_iterator(dir))if(file.path().extension()==L".csv"){++count;total+=file.file_size();require(file.file_size()<=128,"each file respects size cap");std::ifstream in(file.path());std::string header;std::getline(in,header);require(header=="columns","each rotated file carries the header");}
    require(count<=3&&total<=384&&std::filesystem::exists(dir/"keep.txt"),"count/total limits hold and unrelated files are untouched");
    {std::ofstream blocked(root/"blocked");blocked<<"not a directory";}
    require(!log.start(root/"blocked")&&!log.enabled()&&!log.error().empty(),"unwritable or invalid destination is a visible nonfatal failure");
    mc::SessionLog newSession;require(!newSession.enabled(),"new instance never remembers logging enabled");
    require(std::filesystem::weakly_canonical(root).parent_path()==cwd,"test cleanup stays in its own build directory");std::filesystem::remove_all(root);
    std::cout<<"PASS: opt-in recording, no default files, stop/restart, caps, rotation, header and failure handling\n";
}
