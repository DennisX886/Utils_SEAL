#include"utils.h"
#include"nlopt_cal.h"
#include <unistd.h>
using namespace std;
//说明
//作者G3解天行，版本1.
//功能：加密计算lnx函数
//输入值范围：0.9～1.2*1.333^40(119324.79……)(kind 1)，1/119235～0.9(kind 2)
//误差情况：绝对误差保持在0.005以内或者相对误差保持在0.1%以内
/*
    这是计算函数的主函数，目前只包含了lnx计算的部分
    函数逻辑：
        1.输入初始输入值x,确定计算模式
        2.根据计算模式创建utils类实体，在构造函数中实现计算和结果的输出。utils类的具体实现会在"utils.h"中说明和展现
        3.（已经注释掉）调用一个基于nlopt库的，求解最佳拟合多项式的系数的函数。该函数的具体实现会在"nlopt.h"中说明和展现
    函数说明：void menu():一个显示当前工作目录的函数

*/
/*修改：2025.2.21原本两个模式的数据处理，简化到一个模式中.用户只输入一个模式即可
        修改比大小过程，尝试做到不使用解密。没成功*/
/*修改：2025.2.24修改逻辑，将部分数据处理改为明文；这部分之后可能放到前端实现
        系数模数修改为49,30,30,30,30,49,支持带系数的15次方项和系数为1的16次项
        预定加入选择精度的程序*/
/*
修改：2025.2.26 实现了从文件读取系数。预计添加基于雷米兹算法的自行计算系数的程序，并实现所有情况下的逻辑计算*/
/*新计算思路：为了减小后缀的误差，将后缀拆解为5,10,50,100,500,1000…………*/
/*修改：2025.3.3设计了精度算法，支持0～3,后续可能添加3～5或3～7*/
/*修改：2025.3.4 添加了3～5的精度选择模式。关于为什么只添加到5,请参考具体说明
    后续：优化代码，增加可读性和健壮性，减少精度影响*/

vector<int> break_suffix(int x)
{
    vector<int> suffix;
    //转换为string判断位数：
    string str = to_string(x);
    int weishu= str.length();
    //分解
    int time=(weishu-1)*2;
    int devide = pow(10, weishu -1);
    if(x/devide>=5)
    {
        devide*=5;
        time+=1;
        suffix.push_back(time);
        suffix.push_back(x/devide);
        x=x%devide;
        devide/=5;
        
    }
    else
    {
        suffix.push_back(time);
    }
    
    for(int i=0;i<weishu-1;i++)
    {
        suffix.push_back(x/devide);
        x=x%devide;
        devide=devide/2;
        suffix.push_back(x/devide);
        x=x%devide;
        devide=devide/5;
    }
    suffix.push_back(x);
    return suffix;
    //suffix含义：第一位是除法次数，后面是从大到小每一个数字的结果
}
//main函数。调用计算函数
void menu()//一个打印当前工作目录的函数
{
          char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        std::cout << "当前工作目录: " << buffer << std::endl;
    } else {
        std::cerr << "无法获取当前工作目录！" << std::endl;
    }
}
int main(int argc, char *argv[])
{
    //输入
    cout<<"this is SEAL calculate"<<endl;
    cout<<"input number x"<<endl;
    double x;
    //x=stod(argv[1]);
    cin>>x;
    //cout<<"input mode: 1 for lnx"<<endl;
    int kind=1;
    int accuracy;
    //accuracy=stoi(argv[2]);
    //cin>>kind;
    cout<<"input accuracy,1~8"<<endl;
cin>>accuracy;
   double m=0;
            int i=0;
 
    //lnx 范围：0.9～1.2*1.333^40(119324.79……)(kind 1)，1/119235～0.9(kind 2)//2025.2.21修改后：并入到kind1中
    
   //根据模式判断输入数值的合法性
    switch (kind)
    {
        case 1://模式1：计算x>=0.9的lnx
            if(x<0 )
             {
              cout<<"input out of scale"<<endl;
             return 0;
              }
            if(x<=0.9)
            {
                kind=2;
                x=1.0/x;
            }
          
            
            break;

            //其他模式，如指数函数计算模式和三角函数计算模式等，请自行补充设计
        default:
            cout<<"input error"<<endl;
            return 0;
    }
      //创建utils实体
    if(kind==1||kind==2)
    {
         m=x;
            while(true)
            {
                
                if(m>=0.9&&m<=1.2)
                {
                    break;
                }
                m=m/1.333;
                i+=1;
            }
            cout<<"i="<<i<<endl;
            cout<<"m="<<m<<endl;
    vector<int> suffix=break_suffix(i);
    for(size_t i=0;i<suffix.size();i++)
    {
        cout<<suffix[i]<<endl ;

    }
    
    utils utils(kind,x,m,suffix,accuracy);
    }
   // menu();

   // nlopt_lnx();

    return 0;
}
