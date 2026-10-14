// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include <windows.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
int main(int argc, char** argv)
{
    if(argc==2 && std::string(argv[1])=="--child") { Sleep(30000); return 0; }
    if(argc!=5 || std::string(argv[2])!="adc" || std::string(argv[3])!="0" || std::string(argv[4])!="2853114") return 9;
    const char* configured=std::getenv("SIMNODUS_TEST_ADC_MODE");
    const std::string mode=configured?configured:"valid";
    const std::string good="{\"command\":\"adc\",\"before_us\":4010,\"after_us\":4010}";
    if(mode=="timeout" || mode=="runner-loss") { Sleep(30000); return 0; }
    if(mode=="descendant") {
        char path[MAX_PATH]; GetModuleFileNameA(nullptr,path,MAX_PATH);
        std::string command="\""+std::string(path)+"\" --child";
        STARTUPINFOA startup{};startup.cb=sizeof(startup);
        PROCESS_INFORMATION child{};
        if(!CreateProcessA(path,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)) return 8;
        const char* record=std::getenv("SIMNODUS_TEST_ADC_CHILD");
        if(!record) return 8;
        std::ofstream(record)<<child.dwProcessId;
        CloseHandle(child.hProcess);CloseHandle(child.hThread);
    }
    if(mode=="fragmented") { std::cout<<good.substr(0,12)<<std::flush;Sleep(30);std::cout<<good.substr(12)<<std::endl; }
    else if(mode=="malformed") std::cout<<"invalid"<<std::endl;
    else if(mode=="duplicate") std::cout<<good<<"\n"<<good<<std::endl;
    else if(mode=="wrongtime") std::cout<<"{\"command\":\"adc\",\"before_us\":4010,\"after_us\":4011}"<<std::endl;
    else if(mode=="oversize") std::cout<<std::string(5000,'x')<<std::endl;
    else std::cout<<good<<std::endl;
    if(mode=="stderr") std::cerr<<"unexpected diagnostics"<<std::endl;
    if(mode=="valid-but-alive") Sleep(30000);
    return mode=="exit7"?7:0;
}
