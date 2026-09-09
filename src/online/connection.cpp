#include "online/connection.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <stdexcept>
namespace fr::online {
namespace {
std::optional<uint32_t> ipv4(std::string_view value){
 uint32_t result=0;size_t start=0;
 for(int part=0;part<4;++part){auto end=value.find('.',start);if(part==3)end=value.size();else if(end==value.npos)return {};
  auto piece=value.substr(start,end-start);unsigned n=0;if(piece.empty()||piece.size()>3||(piece.size()>1&&piece.front()=='0'))return {};
  auto parsed=std::from_chars(piece.data(),piece.data()+piece.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=piece.data()+piece.size()||n>255)return {};
  result=(result<<8)|n;start=end+1;
 }return result;
}
bool safeKey(std::string_view key){return key.size()>=8&&key.size()<=64&&std::all_of(key.begin(),key.end(),[](unsigned char c){return c>=32&&c!=127;});}
std::string_view trim(std::string_view text){while(!text.empty()&&(text.front()==' '||text.front()=='\r'||text.front()=='\n'||text.front()=='\t'))text.remove_prefix(1);while(!text.empty()&&(text.back()==' '||text.back()=='\r'||text.back()=='\n'||text.back()=='\t'))text.remove_suffix(1);return text;}
#ifdef _WIN32
struct Http {HINTERNET handle=nullptr;explicit Http(HINTERNET h):handle(h){if(!h)throw std::runtime_error("Public address lookup unavailable");}~Http(){WinHttpCloseHandle(handle);}operator HINTERNET()const{return handle;}};
void require(BOOL ok){if(!ok)throw std::runtime_error("Public address lookup unavailable");}
#endif
}
bool validConnectionIPv4(std::string_view address,bool internet){
 auto value=ipv4(address);if(!value||(*value>>24)==0||(*value>>24)>=224)return false;
 if(!internet)return true;
 // IANA non-public ranges must never become the Internet invitation.
 constexpr std::array<std::pair<uint32_t,uint32_t>,13> privateRanges{{
  {0x0a000000,0xff000000},{0x64400000,0xffc00000},{0x7f000000,0xff000000},
  {0xa9fe0000,0xffff0000},{0xac100000,0xfff00000},{0xc0000000,0xffffff00},
  {0xc0000200,0xffffff00},{0xc0586300,0xffffff00},{0xc0a80000,0xffff0000},
  {0xc6120000,0xfffe0000},{0xc6336400,0xffffff00},{0xcb007100,0xffffff00},{0xf0000000,0xf0000000}}};
 // The two public anycast assignments are exceptions in 192.0.0.0/24.
 if(*value==0xc0000009||*value==0xc000000a)return true;
 return std::none_of(privateRanges.begin(),privateRanges.end(),[&](auto range){return (*value&range.second)==range.first;});
}
std::optional<ConnectionInvite> parseConnectionInvite(std::string_view text){
 if(text.size()>160)return {};while(!text.empty()&&(text.back()=='\r'||text.back()=='\n'))text.remove_suffix(1);
 const auto colon=text.find(':'),key=text.find("  Key: ");if(colon==text.npos||key==text.npos||key<=colon)return {};
 auto address=text.substr(0,colon),portText=text.substr(colon+1,key-colon-1),secret=text.substr(key+7);unsigned port=0;
 auto parsed=std::from_chars(portText.data(),portText.data()+portText.size(),port);
 if(!validConnectionIPv4(address)||!safeKey(secret)||parsed.ec!=std::errc{}||parsed.ptr!=portText.data()+portText.size()||!port||port>65535)return {};
 return ConnectionInvite{std::string(address),std::string(secret),uint16_t(port)};
}
std::string connectionInvite(const ConnectionInvite& value,bool internet){
 if(!validConnectionIPv4(value.address,internet)||!value.port||!safeKey(value.key))return {};
 return value.address+":"+std::to_string(value.port)+"  Key: "+value.key;
}
std::string discoverPublicIPv4(){
 // IPv4-only HTTPS endpoint, direct connection: a browser HTTP proxy's
 // exit address would not describe this game's listening TCP socket.
#ifdef _WIN32
 Http session(WinHttpOpen(L"PokeMulti public-address lookup",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 require(WinHttpSetTimeouts(session,2000,2000,2000,2000));DWORD retries=1,protocols=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
 require(WinHttpSetOption(session,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries)));
 require(WinHttpSetOption(session,WINHTTP_OPTION_SECURE_PROTOCOLS,&protocols,sizeof(protocols)));
 Http connection(WinHttpConnect(session,L"api.ipify.org",INTERNET_DEFAULT_HTTPS_PORT,0));
 Http request(WinHttpOpenRequest(connection,L"GET",L"/",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH));
 DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;require(WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects)));
 require(WinHttpSendRequest(request,L"Accept: text/plain\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0));require(WinHttpReceiveResponse(request,nullptr));
 DWORD status=0,size=sizeof(status);require(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));
 if(status!=200)throw std::runtime_error("Public address lookup unavailable");
 std::string body;const auto deadline=GetTickCount64()+4000;
 for(;;){char bytes[64];DWORD count=0;require(WinHttpReadData(request,bytes,sizeof(bytes),&count));if(!count)break;
  if(GetTickCount64()>deadline||body.size()+count>64)throw std::runtime_error("Invalid public address response");body.append(bytes,count);
 }
#else
 CURL* request=curl_easy_init();if(!request)throw std::runtime_error("Public address lookup unavailable");
 struct Cleanup{CURL* handle;~Cleanup(){curl_easy_cleanup(handle);}} cleanup{request};
 std::string body;curl_easy_setopt(request,CURLOPT_URL,"https://api.ipify.org/");curl_easy_setopt(request,CURLOPT_PROXY,"");
 curl_easy_setopt(request,CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4);curl_easy_setopt(request,CURLOPT_TIMEOUT_MS,6000L);curl_easy_setopt(request,CURLOPT_CONNECTTIMEOUT_MS,2000L);
 curl_easy_setopt(request,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(request,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(request,CURLOPT_WRITEFUNCTION,+[](char* data,size_t size,size_t count,void* raw)->size_t{auto& result=*static_cast<std::string*>(raw);if(size&&count>64/size)return 0;size_t n=size*count;if(result.size()+n>64)return 0;result.append(data,n);return n;});
 curl_easy_setopt(request,CURLOPT_WRITEDATA,&body);long status=0;const auto result=curl_easy_perform(request);curl_easy_getinfo(request,CURLINFO_RESPONSE_CODE,&status);
 if(result!=CURLE_OK||status!=200)throw std::runtime_error("Public address lookup unavailable");
#endif
 auto address=std::string(trim(body));if(!validConnectionIPv4(address,true))throw std::runtime_error("Invalid public address response");return address;
}
void PublicAddressLookup::refresh(){
 if(!active_)return;++generation_;address_.clear();state_=State::Looking;wanted_=true;
}
void PublicAddressLookup::tick(bool hosting,uint16_t port){
 if(hosting!=active_||(hosting&&port!=port_)){active_=hosting;port_=port;++generation_;address_.clear();state_=hosting?State::Looking:State::Idle;wanted_=hosting;}
 if(pending_.valid()&&pending_.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=pending_.get();if(active_&&result.generation==generation_){address_=std::move(result.address);state_=address_.empty()?State::Unavailable:State::Ready;}}
 if(active_&&wanted_&&!pending_.valid()){
  auto fetch=fetch_;auto generation=generation_;wanted_=false;
  pending_=std::async(std::launch::async,[fetch=std::move(fetch),generation]{std::string address;try{address=fetch();if(!validConnectionIPv4(address,true))address.clear();}catch(...){address.clear();}return Result{generation,std::move(address)};});
 }
}
}
