#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace fr::game {
// FireRed's original character encoding. Text supplied by the application is
// encoded as text only; it can never inject native text/script control bytes.
inline uint8_t dialogueGlyph(unsigned char c){
    if(c>='A'&&c<='Z')return uint8_t(0xbb+c-'A');
    if(c>='a'&&c<='z')return uint8_t(0xd5+c-'a');
    if(c>='0'&&c<='9')return uint8_t(0xa1+c-'0');
    switch(c){case ' ':return 0;case '!':return 0xab;case '?':return 0xac;case '.':return 0xad;case '-':return 0xae;case '\'':return 0xb4;case ',':return 0xb8;case '/':return 0xba;case ':':return 0xf0;default:return 0xac;}
}
template<class Width> std::vector<uint8_t> dialogueText(const std::string& input,Width width){
    std::vector<uint8_t> out;unsigned used=0,line=0;bool space=false;
    const auto newline=[&]{out.push_back(line++%2?0xfb:0xfe);used=0;};
    for(size_t i=0;i<input.size()&&i<1024;){
        if(input[i]=='\n'){newline();++i;space=false;continue;}
        if(input[i]==' '){space=used!=0;++i;continue;}
        std::vector<uint8_t> word;unsigned length=0;
        while(i<input.size()&&i<1024&&input[i]!=' '&&input[i]!='\n'){
            const auto c=static_cast<unsigned char>(input[i++]);
            if((c&0xc0)==0x80)continue;
            const auto glyph=dialogueGlyph(c);word.push_back(glyph);length+=width(glyph);
        }
        if(used&&used+(space?width(0):0)+length>204){newline();space=false;}
        if(space){out.push_back(0);used+=width(0);}space=false;
        for(auto glyph:word){if(used+width(glyph)>204)newline();out.push_back(glyph);used+=width(glyph);}
    }
    out.push_back(0xff);return out;
}
}
