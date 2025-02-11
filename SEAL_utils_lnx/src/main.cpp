#include"utils.h"
#include"nlopt_cal.h"
using namespace std;
//说明
//作者G3解天行，版本1.
//功能：加密计算lnx函数
//输入值范围：0.9～1.2*1.333^40(119324.79……)(kind 1)，1/119235～0.9(kind 2)
//误差情况：绝对误差保持在0.005以内
/*
    这是计算函数的主函数，目前只包含了lnx计算的部分
    函数逻辑：
        1.输入初始输入值x,确定计算模式
        2.根据计算模式创建utils类实体，在构造函数中实现计算和结果的输出。utils类的具体实现会在"utils.h"中说明和展现
        3.（已经注释掉）调用一个基于nlopt库的，求解最佳拟合多项式的系数的函数。该函数的具体实现会在"nlopt.h"中说明和展现

*/

//main函数。调用计算函数
int main()
{
    //输入
    cout<<"this is SEAL calculate"<<endl;
    cout<<"input number x"<<endl;
    double x;
    cin>>x;
    cout<<"input mode: 1 for lnx"<<endl;
    int kind;
    cin>>kind;
   
 
    //lnx 范围：0.9～1.2*1.333^40(119324.79……)(kind 1)，1/119235～0.9(kind 2)
    
   //根据模式判断输入数值的合法性
    switch (kind)
    {
        case 1://模式1：计算x>=0.9的lnx
            if(x<0.9 ||x>119234.79)
             {
              cout<<"input out of scale"<<endl;
             return 0;
              }
            break;
        case 2://模式2：计算x<0.9的lnx
            if(x<=1.0/119235||x>0.9)
            {
                cout<<"input out of scale"<<endl;
                return 0;
            }
            break;
            //其他模式，如指数函数计算模式和三角函数计算模式等，请自行补充设计
        default:
            cout<<"input error"<<endl;
            return 0;
    }

    //创建utils实体
    utils utils(kind,x);
    //nlopt_lnx();

    return 0;
}