#include<iostream>
#include<cmath>
#include"nlopt.hpp"
#include<vector>
//nlopt是一个非线性优化库，可以用于求解：在特定误差范围和输入数据范围内，给定多项式拟合函数拟合给定函数时，误差最小的多项式的系数。
//这里求解的是数据在0.9~1.2时，ax+b*x*x+c*x*x*x拟合lnx时，拟合效果最好的a,b,c值。误差限制数值为1e-12，a,b,c的大小均限定在正负10以内。程序会打印出a,b,c的估计值和最大误差。
using namespace nlopt;
using namespace std;
double objective(const std::vector<double> &params, std::vector<double> &grad, void *data)
{
    double a =params[0];
    double b =params[1];
    double c =params[2];
    double max_error=0.0;
    double index_of_max=0;
    for(double x=0.9;x<=1.2;x+=(1.2-0.9)/1000)//分为1000项
    {
        double poly_value = a*x+b*x*x+c*x*x*x;//多项式
        double error = abs(log(x)-poly_value);//每一项误差
        if(error>max_error)
        {
            max_error=error;
            index_of_max=x;
        
        }
    }
    return max_error;
}
void nlopt_lnx()
{
    opt opt(GN_DIRECT,3);
    opt.set_min_objective(objective,NULL);

    vector<double> lb={-10.0,-10.0,-10.0};
    vector<double> ub={10.0,10.0,10.0};
    opt.set_lower_bounds(lb);
    opt.set_upper_bounds(ub);

    opt.set_ftol_rel(1e-12);//误差限制。误差限制越小，程序努力输出的误差就会越小

    vector<double> params={1.0,-0.5,1.0/3};//初始a,b,c值

    double minf;

    try
    {
        nlopt::result result_=opt.optimize(params,minf);
        cout<<"a="<<params[0]<<",b="<<params[1]<<",c="<<params[2]<<std::endl;//a,b,c的估计值
        cout<<"maximum absolute error="<<minf<<std::endl;//最大误差
    }catch(exception &e)
    {
        cout<<"failed,"<<e.what()<<std::endl;
    }
    return ;
}