#pragma once
#include "common.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <atomic>
namespace mc {
struct LogLimits{uintmax_t fileBytes=10*1024*1024,totalBytes=100*1024*1024;size_t files=20;};
class SessionLog {
    LogLimits limits_;std::ofstream file_;std::ostringstream pending_;std::filesystem::path directory_,current_;
    std::string header_,error_;uintmax_t bytes_=0;bool enabled_=false;
    inline static std::atomic<unsigned long long> sequence_=0;
    bool fail(const std::string& message){error_=message;enabled_=false;if(file_.is_open())file_.close();pending_.str("");pending_.clear();return false;}
    bool prune(){
        std::error_code ec;struct Entry{std::filesystem::path path;uintmax_t bytes;std::filesystem::file_time_type time;};std::vector<Entry> entries;uintmax_t total=0;
        for(std::filesystem::directory_iterator it(directory_,ec),end;!ec&&it!=end;it.increment(ec)){
            auto name=it->path().filename().wstring();if(!name.starts_with(L"session-")||it->path().extension()!=L".csv"||it->is_symlink(ec)||!it->is_regular_file(ec))continue;
            auto size=it->file_size(ec);auto time=it->last_write_time(ec);if(ec)break;entries.push_back({it->path(),size,time});total+=size;
        }
        if(ec)return fail("Cannot inspect log directory: "+ec.message());
        std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.time==b.time?a.path<b.path:a.time<b.time;});
        size_t count=entries.size();for(const auto& entry:entries){if(count<limits_.files&&total+limits_.fileBytes<=limits_.totalBytes)break;if(!std::filesystem::remove(entry.path,ec)||ec)return fail("Cannot remove oldest log: "+ec.message());total-=entry.bytes;--count;}
        return true;
    }
    bool openPart(){
        if(file_.is_open())file_.close();
        if(!prune())return false;
        current_=directory_/(L"session-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++sequence_)+L".csv");
        file_.clear();file_.open(current_,std::ios::binary|std::ios::trunc);if(!file_)return fail("Cannot create log file");
        file_.write(header_.data(),std::streamsize(header_.size()));bytes_=header_.size();if(!file_)return fail("Cannot write log header");return true;
    }
public:
    explicit SessionLog(LogLimits limits={}):limits_(limits){}
    ~SessionLog(){stop();}
    SessionLog(const SessionLog&)=delete;SessionLog& operator=(const SessionLog&)=delete;
    void setHeader(std::string value){header_=std::move(value);}
    bool start(const std::filesystem::path& directory){
        stop();error_.clear();directory_=directory;pending_.str("");pending_.clear();
        if(limits_.files<1||limits_.fileBytes<=header_.size()||limits_.totalBytes<limits_.fileBytes)return fail("Invalid log limits");
        std::error_code ec;std::filesystem::create_directories(directory_,ec);if(ec)return fail("Cannot create log directory: "+ec.message());
        enabled_=true;if(!openPart())return false;file_.flush();if(!file_)return fail("Cannot flush log header");return true;
    }
    void flush(){
        if(!enabled_)return;
        auto data=pending_.str();pending_.str("");pending_.clear();size_t position=0;
        if(bytes_+data.size()>limits_.fileBytes&&data.size()<=limits_.fileBytes-header_.size())if(!openPart())return;
        while(position<data.size()){
            if(bytes_>=limits_.fileBytes&&!openPart())return;
            size_t size=size_t(std::min<uintmax_t>(data.size()-position,limits_.fileBytes-bytes_));file_.write(data.data()+position,std::streamsize(size));if(!file_){fail("Cannot write log data");return;}position+=size;bytes_+=size;
        }
        file_.flush();if(!file_)fail("Cannot flush log file");
    }
    void stop(){flush();enabled_=false;if(file_.is_open())file_.close();}
    bool enabled() const{return enabled_;}explicit operator bool() const{return enabled_;}
    const std::string& error() const{return error_;}const auto& currentPath() const{return current_;}const auto& directory() const{return directory_;}
    template<class T>SessionLog& operator<<(const T& value){if(enabled_)pending_<<value;return *this;}
    SessionLog& operator<<(std::ostream&(*value)(std::ostream&)){if(enabled_)value(pending_);return *this;}
    SessionLog& operator<<(std::ios_base&(*value)(std::ios_base&)){if(enabled_)value(pending_);return *this;}
};
}
