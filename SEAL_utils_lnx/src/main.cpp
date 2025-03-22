#include"utils.h"
#include"nlopt_cal.h"
#include <unistd.h>
using namespace std;
#define LOW_LNX 0.7
#define HIGH_LNX 1.4
#define ln_LNX 0.693147

#define PI 3.14159265358979323846
#define THRESHOLD 0.12
#define MAX_ITER 3
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
/*修改：2025.3.8 将区间的修改的相关数值改为define，方便修改；更正了一些bug*/
/*修改：2025.3.17 开始开发多功能，添加三角函数计算功能。三角函数代表数字为3（余弦）和4（正弦）*/
/*修改：2025.3.19修改系数模数，精确度进一步提升。继续开发三角函数算法,部分改造了接口，使其适应多种算法*/
/*修改：2025.3.21 三角函数算法报错：重现性化密钥长度不足。不会解决。mlgbd。*/
/*修改：2025.3.21 晚：完善了三角函数算法，现在接口上支持正弦和余弦了。但是重现性化密钥长度还是不足。搞不定。妈蛋。·修改了choose_coeffs函数，为之后指数函数添加了接口*/
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
// 三角函数
//角度归约
double angle_reduction(double x,int &lable) {
    const double two_pi = 2.0 * PI;
    int q = static_cast<int>(x / two_pi + 0.5);
    x= x - q * two_pi;  // 归约到-pai,pai
    //规约到-pai,pai;
    return x;
}
//计算折半次数,按照3倍角折合1/3
int half_break(double x)
{
     int k = 0;
    double y = x < 0 ? -x : x;
    while (y > THRESHOLD && k < MAX_ITER) {
        y /= 3.0;
        k++;
    }
    return k;
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

    cin>>x;
    cout<<"input mode: 1 for lnx,3 for cosx,4 for sinx"<<endl;
    int kind;
    int accuracy;
    cin>>kind;
    cout<<"input accuracy,1~8"<<endl;
cin>>accuracy;
   double m=0;
            int i=0;
    int k=0;//三角函数折半次数
    int lable =0;
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
            if(x<=LOW_LNX)
            {
                kind=2;
                x=1.0/x;
            }
          
            
            break;

            //其他模式，如指数函数计算模式和三角函数计算模式等，请自行补充设计
        case 3://模式3,计算余弦函数
            x=angle_reduction(x,lable);
             k=half_break(x);
            break;
        case 4://模式4，计算正弦函数
            x=x-0.5*PI;
            x=angle_reduction(x,lable);
            k=half_break(x);
            break;
        default:
            cout<<"input error"<<endl;
            return 0;
    }
      //创建utils实体
    if(kind==1||kind==2)
    {
        double ln_num=ln_LNX;
         m=x;
            while(true)
            {
                
                if(m>=LOW_LNX&&m<=HIGH_LNX)
                {
                    break;
                }
                m=m/(HIGH_LNX/LOW_LNX);
               // cout<<"HIGH_LNX/LOW_LNX="<<HIGH_LNX/LOW_LNX<<endl;
                i+=1;
            }
            cout<<"i="<<i<<endl;
            cout<<"m="<<m<<endl;
    vector<int> suffix=break_suffix(i);
    for(size_t i=0;i<suffix.size();i++)
    {
        cout<<suffix[i]<<endl ;

    }
    
    utils utils(kind,x,m,suffix,accuracy,ln_num);
    }
    else if(kind==3||kind==4)
    {
        utils(kind,x,k,accuracy,lable);
    }
   // menu();

   // nlopt_lnx();

    return 0;
}
