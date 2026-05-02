#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
struct Node{std::string op;std::shared_ptr<Node>a,b;};
using N=std::shared_ptr<Node>;
N node(std::string op,N a=nullptr,N b=nullptr){return std::make_shared<Node>(Node{op,a,b});}
struct Parser{
 std::vector<std::string>t;size_t p=0;std::map<std::string,N>wires;std::vector<std::string>inputs,outputs;
 std::string peek(){return p<t.size()?t[p]:"";}
 std::string take(){if(p==t.size())throw std::runtime_error("unexpected EOF");return t[p++];}
 void expect(std::string value){if(take()!=value)throw std::runtime_error("expected "+value);}
 explicit Parser(std::string source){
  source=std::regex_replace(source,std::regex("//[^\n]*"),"");
  std::regex token("1'b[01]|[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[()&,;=|^~]");
  for(auto i=std::sregex_iterator(source.begin(),source.end(),token);i!=std::sregex_iterator();++i)t.push_back(i->str());
 }
 N atom(){auto x=take();if(x=="~")return node("~",atom());if(x=="("){auto e=expression();expect(")");return e;}if(x=="1'b0")x="0";if(x=="1'b1")x="1";return node(x);}
 int precedence(std::string s){return s=="|"?1:s=="^"?2:s=="&"?3:0;}
 N expression(int minimum=1){auto lhs=atom();while(precedence(peek())>=minimum){auto op=take();lhs=node(op,lhs,expression(precedence(op)+1));}return lhs;}
 void parse(){
  expect("module");take();if(peek()=="("){while(take()!=")"){};}expect(";");
  while(peek()!="endmodule"&&!peek().empty()){
   auto kind=take();
   if(kind=="input"||kind=="output"||kind=="wire"){do{auto name=take();if(kind=="input")inputs.push_back(name);if(kind=="output")outputs.push_back(name);}while(peek()==","&&(take(),true));expect(";");}
   else if(kind=="assign"){auto name=take();expect("=");wires[name]=expression();expect(";");}
   else if(kind=="and"||kind=="or"||kind=="xor"||kind=="not"){
    if(peek()!="(")take();expect("(");auto name=take();expect(",");auto a=atom();N b;if(kind!="not"){expect(",");b=atom();}expect(")");expect(";");wires[name]=node(kind=="and"?"&":kind=="or"?"|":kind=="xor"?"^":"~",a,b);
   }else throw std::runtime_error("unsupported statement "+kind);
  }
  expect("endmodule");
 }
 N resolve(N n,std::set<std::string> visiting={}){
  if(!n->a&&wires.count(n->op)){if(!visiting.insert(n->op).second)throw std::runtime_error("combinational loop");return resolve(wires.at(n->op),visiting);}
  return node(n->op,n->a?resolve(n->a,visiting):nullptr,n->b?resolve(n->b,visiting):nullptr);
 }
};
std::string key(N n){return n->a?"("+n->op+key(n->a)+(n->b?key(n->b):"")+")":n->op;}
bool evaluate(N n,const std::map<std::string,bool>&values){
 if(!n->a){if(n->op=="0")return false;if(n->op=="1")return true;return values.at(n->op);}
 bool a=evaluate(n->a,values);if(n->op=="~")return !a;bool b=evaluate(n->b,values);return n->op=="&"?a&&b:n->op=="|"?a||b:a!=b;
}
N optimize(N n,std::map<std::string,N>&cache){
 if(!n->a)return n;
 N a=optimize(n->a,cache),b=n->b?optimize(n->b,cache):nullptr;
 if(b&&key(a)>key(b))std::swap(a,b);
 if(n->op=="~"&&(a->op=="0"||a->op=="1"))return node(a->op=="0"?"1":"0");
 if(b){
  if(n->op=="&"&&(a->op=="0"||b->op=="0"))return node("0");
  if(n->op=="|"&&(a->op=="1"||b->op=="1"))return node("1");
  if(key(a)==key(b))return n->op=="^"?node("0"):a;
  if((n->op=="&"&&a->op=="1")||((n->op=="|"||n->op=="^")&&a->op=="0"))return b;
  if((n->op=="&"&&b->op=="1")||((n->op=="|"||n->op=="^")&&b->op=="0"))return a;
 }
 auto result=node(n->op,a,b);auto k=key(result);if(cache.count(k))return cache[k];return cache[k]=result;
}
std::string verilog(N n){if(!n->a)return n->op=="0"||n->op=="1"?"1'b"+n->op:n->op;if(!n->b)return "~("+verilog(n->a)+")";return "("+verilog(n->a)+n->op+verilog(n->b)+")";}
int main(int argc,char**argv){
 try{
 std::string source;
 if(argc==1)source="module test; input a,b; output y; wire unused; assign unused=a|b; assign y=(a&1'b0)|(b^b); endmodule";
 else{std::ifstream f(argv[1]);if(!f)throw std::runtime_error("file not found");source=std::string(std::istreambuf_iterator<char>(f),{});}
 Parser p(source);p.parse();if(p.inputs.size()>16)throw std::runtime_error("exhaustive checker supports at most 16 inputs");
 std::map<std::string,N>cache;std::ofstream v("optimized.v"),dot("graph.dot");v<<"module optimized;\n";
 for(auto&i:p.inputs)v<<"input "<<i<<";\n";dot<<"digraph netlist {\n";
 for(auto&o:p.outputs){
  N original=p.resolve(node(o)),result=optimize(original,cache);
  for(unsigned mask=0;mask<(1u<<p.inputs.size());mask++){std::map<std::string,bool>values;for(size_t i=0;i<p.inputs.size();i++)values[p.inputs[i]]=(mask>>i)&1;if(evaluate(original,values)!=evaluate(result,values))throw std::runtime_error("equivalence failed");}
  v<<"output "<<o<<";\nassign "<<o<<"="<<verilog(result)<<";\n";
  dot<<"\""<<o<<"\" -> \""<<key(result)<<"\";\n";
 }
 for(auto&entry:cache){auto n=entry.second;if(n->a)dot<<"\""<<entry.first<<"\" -> \""<<key(n->a)<<"\";\n";if(n->b)dot<<"\""<<entry.first<<"\" -> \""<<key(n->b)<<"\";\n";}
 v<<"endmodule\n";dot<<"}\n";std::cout<<"PASS exhaustive equivalence\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
