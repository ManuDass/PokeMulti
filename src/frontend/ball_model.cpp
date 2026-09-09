#include "frontend/ball_model.hpp"
#include "platform/text.hpp"
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#else
#include <libxml/parser.h>
#endif
#include <map>
#include <memory>
#include <sstream>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace fr {
namespace {
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif
struct Node {
    std::string name,text;std::map<std::string,std::string> attrs;std::vector<std::unique_ptr<Node>> children;
    std::string attr(const char* key)const{auto i=attrs.find(key);return i==attrs.end()?"":i->second;}
    const Node* child(const char* key)const{for(const auto& c:children)if(c->name==key)return c.get();return nullptr;}
};
void ensure(bool ok){if(!ok)throw std::runtime_error("Cannot read the supplied Pok\xc3\xa9 Ball Plus model.");}
std::unique_ptr<Node> xml(const std::filesystem::path& path){
    ensure(std::filesystem::file_size(path)<=8*1024*1024);
#ifdef _WIN32
    ComPtr<IStream> stream;ensure(SUCCEEDED(SHCreateStreamOnFileEx(path.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,&stream)));
    ComPtr<IXmlReader> reader;ensure(SUCCEEDED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(reader.GetAddressOf()),nullptr)));
    reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit);
    reader->SetProperty(XmlReaderProperty_MaxElementDepth,64);ensure(SUCCEEDED(reader->SetInput(stream.Get())));
    auto root=std::make_unique<Node>();std::vector<Node*> stack{root.get()};XmlNodeType type;HRESULT hr;size_t count=0;
    while((hr=reader->Read(&type))==S_OK){
        const wchar_t* value=nullptr;UINT length=0;
        if(type==XmlNodeType_Element){
            ensure(++count<=50000);auto node=std::make_unique<Node>();reader->GetLocalName(&value,&length);node->name=narrow(std::wstring(value,length));
            const bool empty=reader->IsEmptyElement()!=FALSE;
            if(reader->MoveToFirstAttribute()==S_OK){do{
                reader->GetLocalName(&value,&length);const auto name=narrow(std::wstring(value,length));reader->GetValue(&value,&length);node->attrs[name]=narrow(std::wstring(value,length));
            }while(reader->MoveToNextAttribute()==S_OK);reader->MoveToElement();}
            auto* raw=node.get();stack.back()->children.push_back(std::move(node));if(!empty)stack.push_back(raw);
        }else if(type==XmlNodeType_EndElement){ensure(stack.size()>1);stack.pop_back();}
        else if(type==XmlNodeType_Text||type==XmlNodeType_CDATA||type==XmlNodeType_Whitespace){
            reader->GetValue(&value,&length);stack.back()->text+=narrow(std::wstring(value,length));
        }
    }
    ensure(hr==S_FALSE&&stack.size()==1);return root;
#else
    xmlDoc* doc=xmlReadFile(path.c_str(),nullptr,XML_PARSE_NONET|XML_PARSE_NOBLANKS);ensure(doc!=nullptr);
    struct Cleanup{xmlDoc* doc;~Cleanup(){xmlFreeDoc(doc);}} cleanup{doc};ensure(!doc->intSubset&&!doc->extSubset);
    auto root=std::make_unique<Node>();size_t count=0;
    auto visit=[&](auto&& self,xmlNode* input,Node& parent,unsigned depth)->void{ensure(depth<=64);for(auto* p=input;p;p=p->next){if(p->type==XML_ELEMENT_NODE){ensure(++count<=50000);auto node=std::make_unique<Node>();node->name=reinterpret_cast<const char*>(p->name);for(auto* a=p->properties;a;a=a->next){xmlChar* value=xmlNodeListGetString(doc,a->children,1);if(value){node->attrs[reinterpret_cast<const char*>(a->name)]=reinterpret_cast<const char*>(value);xmlFree(value);}}self(self,p->children,*node,depth+1);parent.children.push_back(std::move(node));}else if(p->type==XML_TEXT_NODE||p->type==XML_CDATA_SECTION_NODE){if(p->content)parent.text+=reinterpret_cast<const char*>(p->content);}else ensure(p->type!=XML_ENTITY_REF_NODE);}};
    visit(visit,xmlDocGetRootElement(doc),*root,0);return root;
#endif
}
template<class T> std::vector<T> numbers(const Node* n){
    ensure(n!=nullptr);std::istringstream stream(n->text);std::vector<T> v;T x;
    while(stream>>x){ensure(v.size()<1000000);v.push_back(x);}ensure(stream.eof());return v;
}
std::string ref(std::string s){ensure(!s.empty()&&s[0]=='#');return s.substr(1);}
}
void BallModel::load(const std::filesystem::path& dae){
    auto doc=xml(dae);const auto* collada=doc->child("COLLADA");ensure(collada!=nullptr);
    const auto* library=collada->child("library_geometries");ensure(library!=nullptr);
    std::vector<Vertex> result;
    for(const auto& geometry:library->children){
        if(geometry->name!="geometry")continue;const auto* mesh=geometry->child("mesh");ensure(mesh!=nullptr);
        struct Source {std::vector<float> values;size_t stride=0;};std::map<std::string,Source> sources;
        for(const auto& source:mesh->children)if(source->name=="source"){
            const auto* common=source->child("technique_common");ensure(common!=nullptr);const auto* accessor=common->child("accessor");ensure(accessor!=nullptr);
            const auto stride=std::stoul(accessor->attr("stride"));ensure(stride>0&&stride<=16);
            auto values=numbers<float>(source->child("float_array"));for(auto v:values)ensure(std::isfinite(v)&&std::abs(v)<1e6f);
            ensure(values.size()%stride==0);sources.emplace(source->attr("id"),Source{std::move(values),stride});
        }
        const auto* verts=mesh->child("vertices");ensure(verts!=nullptr);std::string positions;
        for(const auto& input:verts->children)if(input->name=="input"&&input->attr("semantic")=="POSITION")positions=ref(input->attr("source"));
        ensure(!positions.empty());
        for(const auto& poly:mesh->children){
            if(poly->name!="polylist"&&poly->name!="triangles")continue;
            struct Input {size_t offset=0;std::string source;};Input pos,uv,normal;bool hasPos=false,hasUv=false,hasNormal=false;size_t stride=0;
            for(const auto& input:poly->children)if(input->name=="input"){
                const size_t offset=std::stoul(input->attr("offset"));ensure(offset<16);stride=std::max(stride,offset+1);
                const auto semantic=input->attr("semantic");
                if(semantic=="VERTEX"){ensure(ref(input->attr("source"))==verts->attr("id"));pos={offset,positions};hasPos=true;}
                else if(semantic=="TEXCOORD"&&(input->attr("set").empty()||input->attr("set")=="0")){uv={offset,ref(input->attr("source"))};hasUv=true;}
                else if(semantic=="NORMAL"){normal={offset,ref(input->attr("source"))};hasNormal=true;}
            }
            ensure(hasPos&&hasUv&&hasNormal&&stride);auto indices=numbers<size_t>(poly->child("p"));
            std::vector<size_t> counts;if(poly->name=="polylist")counts=numbers<size_t>(poly->child("vcount"));else {const size_t count=std::stoul(poly->attr("count"));ensure(count<=100000);counts.assign(count,3);}
            auto value=[&](const Input& in,size_t index,size_t component){
                ensure(index+in.offset<indices.size());const auto s=sources.find(in.source);ensure(s!=sources.end());const auto& src=s->second;
                ensure(component<src.stride&&indices[index+in.offset]<src.values.size()/src.stride);
                return src.values[indices[index+in.offset]*src.stride+component];
            };
            auto vertex=[&](size_t index){return Vertex{value(pos,index,0),value(pos,index,1),value(pos,index,2),value(uv,index,0),value(uv,index,1),value(normal,index,0),value(normal,index,1),value(normal,index,2)};};
            size_t cursor=0;for(const auto count:counts){ensure(count>=3&&count<=16&&cursor+count*stride<=indices.size());
                for(size_t k=1;k+1<count;++k){ensure(result.size()<300000);result.push_back(vertex(cursor));result.push_back(vertex(cursor+k*stride));result.push_back(vertex(cursor+(k+1)*stride));}
                cursor+=count*stride;
            }ensure(cursor==indices.size());
        }
    }
    ensure(!result.empty());
    // Both supplied materials use this original diffuse atlas. Preview the mesh
    // in its exported, closed-ball bind pose; do not deform or replace any part.
    auto texture=readImage(dae.parent_path()/"ob0008_00_obj_col.png");
    vertices_=std::move(result);texture_=std::move(texture);
}
Image BallModel::render(float yaw,float pitch,int width,int height)const{
    if(empty())return {};ensure(width>=32&&height>=32&&width<=1024&&height<=1024&&std::isfinite(yaw)&&std::isfinite(pitch));
    struct V {float x,y,z,u,v,light;};std::vector<V> v;v.reserve(vertices_.size());
    auto rotate=[&](float x,float y,float z){const float xx=x*std::cos(yaw)+z*std::sin(yaw),zz=-x*std::sin(yaw)+z*std::cos(yaw);return std::array<float,3>{xx,y*std::cos(pitch)-zz*std::sin(pitch),y*std::sin(pitch)+zz*std::cos(pitch)};};
    float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;
    for(const auto& a:vertices_){
        const auto p=rotate(a.x,a.y,a.z),n=rotate(a.nx,a.ny,a.nz);const float inv=1.f/(60.f-p[2]);
        const float light=.7f+.3f*std::max(0.f,(-.3f*n[0]+.5f*n[1]+.8f*n[2])/std::max(.001f,std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2])));
        v.push_back({p[0]*inv,-p[1]*inv,inv,a.u*inv,a.v*inv,light*inv});
        x0=std::min(x0,v.back().x);x1=std::max(x1,v.back().x);y0=std::min(y0,v.back().y);y1=std::max(y1,v.back().y);
    }
    const float scale=std::min((width-20.f)/std::max(.001f,x1-x0),(height-20.f)/std::max(.001f,y1-y0));
    for(auto& a:v){a.x=(a.x-(x0+x1)*.5f)*scale+width*.5f;a.y=(a.y-(y0+y1)*.5f)*scale+height*.5f;}
    Image image{width,height,std::vector<uint32_t>(size_t(width)*height)};std::vector<float> depth(image.pixels.size());
    auto edge=[](const V& a,const V& b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
    for(size_t t=0;t<v.size();t+=3){const auto& a=v[t];const auto& b=v[t+1];const auto& c=v[t+2];const float area=edge(a,b,c.x,c.y);if(std::abs(area)<.0001f)continue;
        const int left=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),right=std::min(width-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
        const int top=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),bottom=std::min(height-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){
            const float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;if(wa<-.00001f||wb<-.00001f||wc<-.00001f)continue;
            const float inv=wa*a.z+wb*b.z+wc*c.z;const size_t i=size_t(y)*width+x;if(inv<=depth[i])continue;
            const float u=(wa*a.u+wb*b.u+wc*c.u)/inv,vv=(wa*a.v+wb*b.v+wc*c.v)/inv,light=(wa*a.light+wb*b.light+wc*c.light)/inv;
            const int tx=std::clamp(int((u-std::floor(u))*texture_.width),0,texture_.width-1),ty=std::clamp(int((1.f-(vv-std::floor(vv)))*texture_.height),0,texture_.height-1);
            const auto tex=texture_.pixels[size_t(ty)*texture_.width+tx];if((tex>>24)<128)continue;
            uint32_t pixel=0xff000000;for(int shift:{0,8,16})pixel|=uint32_t(std::clamp(float((tex>>shift)&255)*light,0.f,255.f))<<shift;
            image.pixels[i]=pixel;depth[i]=inv;
        }
    }
    return image;
}
}
