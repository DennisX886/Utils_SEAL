#include"seal/seal.h"
#include<string>
#include<iostream>
#include<vector>
#include<cmath>
#include<fstream>

#define EXP_UP_LIMIT 680
#define EXP_LOW_LIMIT 0
#define TRI_UP_LIMIT 16
#define TRI_LOW_LIMIT -16
#define FIX 2
using namespace std;
using namespace seal;
//说明
/*
    utils类封装了数个成员函数和变量，可以实现SEAL库的初始化，数据的加密，结果的解密和基于密文的lnx计算功能
    成员函数说明：
        utils(int kind,double x):构造函数，可以实现初始化，加密，计算，解密流程。输入变量分别为模式kind和初始输入值x
        ~utils():析构函数
        void init(int kind):初始化函数。将SEAL库的相关参数初始化，生成上下文，密钥，加密/解密/计算器，解码器等。输入值为模式kind.
        void encrypt_input(double x,int kind):变量加密函数，可以将输入变量x加密为密文。输入值为初始输入值x和模式kind.需要注意的是，只能用于加密x变量，不能用于加密其他变量。
        void decrypt_result(Ciphertext c_r,int kind):结果解密函数，可以将密文解密，并打印。输入值为密文c_r和模式kind.
        void decrypt_check(Ciphertext c,string name):变量解密函数，可以解密任意密文并打印结果，用于检查加密情况。输入值为密文c和密文名称name.
        void lnx_cal(Ciphertext c_m,Ciphertext c_i,int kind，double ln_num):lnx计算函数，可以根据输入值近似计算并打印对应lnx的值。输入值为密文c_m\c_i,模式kind和后缀的lnx的值ln_num.
        void lnx_compare(double x):lnx计算结果比较函数，可以计算真实结果和相对误差，并打印。输入值为初始输入值x.
        void encrypt_input_lnx(double m,vector<int>suffix_ln133):专门用于对数运算的加密函数，接受对数运算传入的参数。
        void choose_coeffs(int accuracy,int kind)：用于读取文件系数的函数。输入为精度和类型
        void check_text(t t_r,string name):用于检查模数转换链索引和缩放因子大小的函数。输入为明文或密文对象，以及他们的名字（用于打印显示）
    

*/

class utils
{

   private:
    EncryptionParameters *p_parms;
    size_t poly_modulus_degree=16384;//8192;
    //size_t poly_modulus_degree_2=16284;
    SEALContext * p_context ;//上下文指针
    KeyGenerator * p_keygen;//密钥生成器指针
    Encryptor * p_encryptor;//加密器指针
    Decryptor * p_decryptor;//解密器指针
    Evaluator * p_evaluator;//计算器指针
    CKKSEncoder * p_encoder;//编码器指针
    PublicKey public_key;//公钥
    RelinKeys relin_keys;//线性化密钥
    double scale;//缩放因子
   // double sign_poly_coeff=0.763546;//比较大小参数
    const vector<int> poly_coeffs_lnx = {60,40,40,40,40,60};//{49,30,30,30,30,49};///系数模数
    const vector<int> poly_coeffs_tri = {60,40,40,40,60};
    const vector<int> poly_coeffs_exp = {60,50,50,50,50,50,50,60};
   //const vector<int> poly_coeffs_2 = {98,60,60,60,60,98};
    Plaintext p_x;//初始输入值明文
    Ciphertext c_x;//初始输入值密文
    Plaintext p_r; //结果明文 
    vector<double> result;//最终结果
    vector<double> plain_coeffs;//加密前的系数
    vector<Plaintext> coeff_p;//明文状态下的系数
    vector<Ciphertext> levs;//密文状态下的,展开公式的每一项
    vector<Ciphertext> levs_ln133;//尾数多项式密文
    vector<Plaintext> plain_coeffs_ln133;//拆分出来的尾数多项式
    //lnx计算相关。用法见lnx_cal函数。
    Ciphertext c_m;//尾数密文 
    Ciphertext c_i;//次数密文，现在弃用
    Plaintext p_m;//尾数明文
    Plaintext p_i;//次数明文，现在用于过渡数据
    //Plaintext p_sign_poly_coeff;//比较大小明文
   

public:
//重载构造函数1：专门为三角函数设计
    utils(int kind,double x,int accuracy,int lable)
   { 
    init(kind);
    double a=x*10;
    a=(int)a*0.1;
    double b=a+0.1;
    cout<<"a="<<a<<endl;
    choose_coeffs(accuracy,kind,a,b);
    encrypt_input(x);
    cosx_cal(kind,accuracy,lable);
    tri_compare(x,kind,lable);
    
    }
    //重载构造函数2：专门为lnx设计
    utils(int kind,double x,double m,vector<int>suffix_ln133,int accuracy,double ln_num)
    {
        init(kind);
        choose_coeffs(accuracy,kind,0,0);
        encrypt_input_lnx(m,suffix_ln133);
        lnx_cal(c_m,suffix_ln133[0],kind,accuracy,ln_num);
        lnx_compare(x,kind);

    }
    utils(int kind,int lable,double x,int accuracy,int time)//重载构造函数3,专门为指数函数设计
    {
        init(kind);
        double a=x*10;
        a=(int)a*0.1;
        choose_coeffs(accuracy,kind,a,0);
        encrypt_input(x);
        exp_cal(accuracy,lable);
        exp_compare(x,lable,time);
    }
    ~utils()
    {
        delete p_parms;
        delete p_context;
        delete p_keygen;
        delete p_encryptor;
        delete p_decryptor;
        delete p_evaluator;
        delete p_encoder;
    }
   void init (int kind)
   {
    p_parms=new EncryptionParameters(scheme_type::ckks);
    p_parms->set_poly_modulus_degree(poly_modulus_degree);
    //以下判断逻辑针对系数模数和缩放因子，根据lnx计算要求设定了专门的系数模数和缩放因子。指数和三角函数运算等其他情况请自行补充
    if(kind==1||kind==2)
    {
       p_parms->set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, poly_coeffs_lnx)); 
       scale=pow(2.0,poly_coeffs_lnx[1]);
    }
    else if(kind==3||kind==4)
    {
        p_parms->set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree,poly_coeffs_tri));
        scale = pow(2.0,poly_coeffs_tri[1]);
 //       p_parms->set_decomposition_bit_count(60);
    }
    else if(kind==5)
    {
        p_parms->set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree,poly_coeffs_exp));
        scale = pow(2.0,poly_coeffs_exp[1]);
    }
    p_context =  new SEALContext (*p_parms, true, seal::sec_level_type::tc128);
    p_keygen= new KeyGenerator(*p_context);
    auto secret_key=p_keygen->secret_key();
    p_keygen->create_public_key(public_key);
    p_keygen->create_relin_keys(relin_keys);
   // relin_keys=p_keygen->create_relin_keys(3,false);
    p_evaluator=new Evaluator(*p_context);
    p_encryptor=new Encryptor(*p_context, public_key);
    p_decryptor=new Decryptor(*p_context, secret_key);
    p_encoder=new CKKSEncoder(*p_context);
   } 

   void choose_coeffs(int accuracy,int kind,double a,double b)
    {
        string name;
        cout<<"kind="<<kind<<endl;
        if(kind==1||kind==2)
        {
            name="COEFFLIST_LNX";
        }
        else if(kind==3||kind==4)
        {
            name ="COEFFLIST_TRI";
        }
        else if(kind==5)
        {
            name="COEFFLIST_EXP";
        }
        fstream file(name);
        if(!file.is_open())
        {
            cerr<<"无法读取系数文件"<<endl;
            return ;
        }
        string line;
        double line_number=0;
        int limit=0;
        if(kind==1||kind==2)
        {
            limit = accuracy;
        }
        else if(kind==5)
        {
            limit = (accuracy-1)*(10*EXP_UP_LIMIT-10*EXP_LOW_LIMIT+1)+10*a-10*EXP_LOW_LIMIT+1;
        }
        else
        {
            limit = (accuracy-1)*(TRI_UP_LIMIT-TRI_LOW_LIMIT+1)+10*a-10*TRI_LOW_LIMIT+1;;
        }
        cout<<"limit="<<limit<<endl;
        while(line_number<limit)
        {
            getline(file,line);
           line_number++;
        }
        if(line_number==limit)
        {
            istringstream iss(line);
            double num;
            while(iss>>num)
            {
                plain_coeffs.push_back(num);
            }
            if((int)plain_coeffs.size()==0)//这时，没有数据，需要自行创建写入.现在只有指数函数算法支持该功能，但是还没更新上。得了后面再实现吧，没必要
            {
              //  if(kind==5)
              //  {
              //      int down_limti = x_;
              //      int up_limit=x_+1;
              //      plain_coeffs=remez(down_limti,up_limit,accuracy);

               // }
             //   cout<<"系数为空，请检查系数文件"<<endl;
            //    return;
            }
        }
        file.close();
        cout<<"系数为：";
        for(int i=0;i<(int)plain_coeffs.size();i++)
        {
            cout<<plain_coeffs[i]<<"    ";
        }
        cout<<endl;

    }
     void encrypt_input(double x)
        {
            cout<<x<<endl;
            p_encoder->encode(x, scale, p_x);
            p_encryptor->encrypt(p_x, c_x);
        }
    void encrypt_input_lnx(double m,vector<int>suffix_ln133)
    {
        levs_ln133.resize(suffix_ln133.size()-1);
        int index=1;
        for(int i=0;i<suffix_ln133[0]+1;i++)
        {
            p_encoder->encode(suffix_ln133[index],scale,p_i);
            p_encryptor->encrypt(p_i,levs_ln133[index-1]);
            index++;
        }
        cout<<"index="<<index<<endl;
        p_encoder->encode(m,scale,p_m);
        p_encryptor->encrypt(p_m,c_m);
    }
    void decrypt_result(Ciphertext c_r,int kind)
    {
        p_decryptor->decrypt(c_r,p_r);
        p_encoder->decode(p_r,result);
        if(kind==2)
        {
            result[0]=-result[0];
        }
        if(kind==1)
        {
            result[0]=1.0/result[0];
        }
        
       
            cout<<"result is "<<result[0]<<endl;
   
    }
    void exp_cal(int accuracy,int lable)
    {
        
         coeff_p.resize(plain_coeffs.size());
        levs.resize(plain_coeffs.size());
         for(int i=0;i<(int)plain_coeffs.size();i++)
         {
             p_encoder->encode(plain_coeffs[i],scale,coeff_p[i]);
             decrypt_check(coeff_p[i],"coeff_p[i]");
         }
         cout<<"plaintext 1 ready"<<endl;

         Plaintext coeff_p_1;
         p_encoder->encode(1,scale,coeff_p_1);
         cout<<"plaintext 2 ready"<<endl;
         Ciphertext xWith1,x2,x4;
         decrypt_check(c_x,"c_x");

         p_encryptor->encrypt(coeff_p[0],levs[0]);
         //cout<<"levs0 ready"<<endl;
         decrypt_check(levs[0],"levs[0]");

         if(accuracy>=1)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[1],levs[1]);
            p_evaluator->rescale_to_next_inplace(levs[1]);
            decrypt_check(levs[1],"levs[1]");
         }

         if(accuracy>=2)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[2],levs[2]);
            p_evaluator->rescale_to_next_inplace(levs[2]);

            p_evaluator->multiply_plain(c_x,coeff_p_1,xWith1);
            p_evaluator->rescale_to_next_inplace(xWith1);

            p_evaluator->multiply_inplace(levs[2],xWith1);
            p_evaluator->relinearize_inplace(levs[2],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[2]);
            decrypt_check(levs[2],"levs[2]");
         }

         if(accuracy>=3)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[3],levs[3]);
            p_evaluator->rescale_to_next_inplace(levs[3]);

            p_evaluator->multiply(c_x,c_x,x2);
            p_evaluator->relinearize_inplace(x2,relin_keys);
            p_evaluator->rescale_to_next_inplace(x2);

            p_evaluator->multiply_inplace(levs[3],x2);
            p_evaluator->relinearize_inplace(levs[3],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[3]);
            decrypt_check(levs[3],"levs[3]");
         }

        if(accuracy>=4)
        {
            p_evaluator->multiply_plain(c_x,coeff_p[4],levs[4]);
            p_evaluator->rescale_to_next_inplace(levs[4]);

            p_evaluator->multiply_inplace(levs[4],x2);
            p_evaluator->relinearize_inplace(levs[4],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[4]);
            decrypt_check(levs[4],"levs[4]_coeff*x^3");
            Ciphertext c_x_help = c_x;
            p_evaluator->mod_switch_to_inplace(c_x_help,levs[4].parms_id());

            p_evaluator->multiply_inplace(levs[4],c_x_help);
            p_evaluator->relinearize_inplace(levs[4],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[4]);
            decrypt_check(levs[4],"levs[4]");
        }
        if(accuracy>=5)
        {
            p_evaluator->multiply_plain(c_x,coeff_p[5],levs[5]);
            p_evaluator->rescale_to_next_inplace(levs[5]);

            p_evaluator->multiply(x2,x2,x4);
            p_evaluator->relinearize_inplace(x4,relin_keys);
            p_evaluator->rescale_to_next_inplace(x4);

            p_evaluator->mod_switch_to_inplace(levs[5],x4.parms_id());

            p_evaluator->multiply_inplace(levs[5],x4);
            p_evaluator->relinearize_inplace(levs[5],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[5]);

            decrypt_check(levs[5],"levs[5]");
    }
       for(int i=0;i<accuracy+1;i++)
        {
            levs[i].scale()=scale;
            if(levs[i].parms_id()!=levs[accuracy].parms_id())
            {
                p_evaluator->mod_switch_to_inplace(levs[i],levs[accuracy].parms_id());
            }
        }
        Ciphertext c_r=levs[0];
        for(int i=1;i<accuracy+1;i++)
        {
            p_evaluator->add_inplace(c_r,levs[i]);
        }
        decrypt_result(c_r,lable);
    }
    void cosx_cal(int kind,int accuracy,int lable)
    {
        coeff_p.resize(plain_coeffs.size());
        levs.resize(plain_coeffs.size());
         for(int i=0;i<(int)plain_coeffs.size();i++)
         {
             p_encoder->encode(plain_coeffs[i],scale,coeff_p[i]);
         }
         cout<<"plaintext 1 ready"<<endl;

         Plaintext coeff_p_1;
         p_encoder->encode(1,scale,coeff_p_1);
         cout<<"plaintext 2 ready"<<endl;
         Ciphertext xWith1,x2,x4;

         p_encryptor->encrypt(coeff_p[0],levs[0]);
         cout<<"levs0 ready"<<endl;

         if(accuracy>=1)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[1],levs[1]);
            p_evaluator->rescale_to_next_inplace(levs[1]);
            decrypt_check(levs[1],"levs[1]");
         }

         if(accuracy>=2)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[2],levs[2]);
            p_evaluator->rescale_to_next_inplace(levs[2]);

            p_evaluator->multiply_plain(c_x,coeff_p_1,xWith1);
            p_evaluator->rescale_to_next_inplace(xWith1);

            p_evaluator->multiply_inplace(levs[2],xWith1);
            p_evaluator->relinearize_inplace(levs[2],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[2]);
            decrypt_check(levs[2],"levs[2]");
         }

         if(accuracy>=3)
         {
            p_evaluator->multiply_plain(c_x,coeff_p[3],levs[3]);
            p_evaluator->rescale_to_next_inplace(levs[3]);

            p_evaluator->multiply(c_x,c_x,x2);
            p_evaluator->relinearize_inplace(x2,relin_keys);
            p_evaluator->rescale_to_next_inplace(x2);

            p_evaluator->multiply_inplace(levs[3],x2);
            p_evaluator->relinearize_inplace(levs[3],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[3]);
            decrypt_check(levs[3],"levs[3]");
         }

        if(accuracy>=4)
        {
            p_evaluator->multiply_plain(c_x,coeff_p[4],levs[4]);
            p_evaluator->rescale_to_next_inplace(levs[4]);

            p_evaluator->multiply_inplace(levs[4],x2);
            p_evaluator->relinearize_inplace(levs[4],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[4]);

            Ciphertext c_x_help = c_x;
            p_evaluator->mod_switch_to_inplace(c_x_help,levs[4].parms_id());

            p_evaluator->multiply_inplace(levs[4],c_x_help);
            p_evaluator->relinearize_inplace(levs[4],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[4]);
            decrypt_check(levs[4],"levs[4]");
        }
        if(accuracy>=5)
        {
            p_evaluator->multiply_plain(c_x,coeff_p[5],levs[5]);
            p_evaluator->rescale_to_next_inplace(levs[5]);

            p_evaluator->multiply(x2,x2,x4);
            p_evaluator->relinearize_inplace(x4,relin_keys);
            p_evaluator->rescale_to_next_inplace(x4);

            p_evaluator->mod_switch_to_inplace(levs[5],x4.parms_id());

            p_evaluator->multiply_inplace(levs[5],x4);
            p_evaluator->relinearize_inplace(levs[5],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[5]);

            decrypt_check(levs[5],"levs[5]");
        }

    //计算泰勒的相加
        for(int i=0;i<accuracy+1;i++)
        {
            levs[i].scale()=scale;
            if(levs[i].parms_id()!=levs[accuracy].parms_id())
            {
                p_evaluator->mod_switch_to_inplace(levs[i],levs[accuracy].parms_id());
            }
        }
        Ciphertext c_r=levs[0];
        for(int i=1;i<accuracy+1;i++)
        {
            p_evaluator->add_inplace(c_r,levs[i]);
        }
        decrypt_result(c_r,lable);

        // //首先计算折半之后的值，然后按照折半次数翻回去
        // //准备明文
        // //decrypt_check(c_x,"c_x");
        // //修改逻辑：利用雷米字算法逼近
        //coeff_p.resize(plain_coeffs.size());
        //levs.resize(plain_coeffs.size());
        //  for(int i=0;i<(int)plain_coeffs.size();i++)
        //  {
        //      p_encoder->encode(plain_coeffs[i],scale,coeff_p[i]);
        //  }
        //  cout<<"plaintext 1 ready"<<endl;
        // Plaintext plain_coeff_3,plain_coeff_4,plain_coeff_1,plain_coeff_minus1;
        // p_encoder->encode(-3,scale,plain_coeff_3);
        // p_encoder->encode(4,scale,plain_coeff_4);
        // p_encoder->encode(plain_coeffs[0],scale,plain_coeff_1);
        // p_encoder->encode(-1*plain_coeffs[0],scale,plain_coeff_minus1);
        // cout<<"plaintext 2 ready"<<endl;
        // Ciphertext xWith1,x2,xWithMinus1;
        // //泰勒计算
        // p_encryptor->encrypt(plain_coeff_1,levs[0]);//一阶
        // cout<<"lev[0] ready"<<endl;
        // if(accuracy>=2)
        // {

        //     p_evaluator->multiply_plain(c_x,coeff_p[1],levs[1]);
        //     p_evaluator->rescale_to_next_inplace(levs[1]);
        //     p_evaluator->multiply_plain(c_x,plain_coeff_1,xWithMinus1);

        //     p_evaluator->rescale_to_next_inplace(xWithMinus1); 
        //                cout<<"-x ready"<<endl;
        //     p_evaluator->multiply_inplace(levs[1],xWithMinus1);
        //     p_evaluator->relinearize_inplace(levs[1],relin_keys);
        //     p_evaluator->rescale_to_next_inplace(levs[1]);

        //  //   check_text(levs[1],"levs[1]");
        //     decrypt_check(levs[1],"levs[1]");
        // }
        // if(accuracy>=3)
        // {
        //     p_evaluator->multiply_plain(c_x,coeff_p[2],levs[2]);
        //     p_evaluator->rescale_to_next_inplace(levs[2]);
        //     p_evaluator->multiply_plain(c_x,plain_coeff_1,xWith1);
        //     p_evaluator->rescale_to_next_inplace(xWith1);
        //     p_evaluator->multiply_inplace(levs[2],xWith1);
        //     p_evaluator->relinearize_inplace(levs[2],relin_keys);
        //     p_evaluator->rescale_to_next_inplace(levs[2]);
        //     p_evaluator->multiply(xWithMinus1,xWithMinus1,x2);
        //     p_evaluator->relinearize_inplace(x2,relin_keys);
        //     p_evaluator->rescale_to_next_inplace(x2);
        //     p_evaluator->multiply_inplace(levs[2],x2);
        //     p_evaluator->relinearize_inplace(levs[2],relin_keys);
        //     p_evaluator->rescale_to_next_inplace(levs[2]);

        // //    check_text(levs[2],"levs[2]");
        //     decrypt_check(levs[2],"levs[2]");
        // }

        // //计算泰勒的相加
        // for(int i=0;i<accuracy;i++)
        // {
        //     levs[i].scale()=scale;
        //     if(levs[i].parms_id()!=levs[accuracy-1].parms_id())
        //     {
        //         p_evaluator->mod_switch_to_inplace(levs[i],levs[accuracy-1].parms_id());
        //     }
            
        // }
        
        // Ciphertext tyler_result;
        // tyler_result=levs[0];
        // for(int i=1;i<(int)levs.size();i++)
        // {
        //     p_evaluator->add_inplace(tyler_result,levs[i]);
        // }
        // decrypt_check(tyler_result,"tyler_result");
        // //接下来使用三倍角公式放大
        // Ciphertext t2,three1,three2,three0;
        
        // three0=tyler_result;
        // Ciphertext big;
        
        // for(int i=0;i<k;i++)
        // {   
        //     p_evaluator->mod_switch_to_inplace(plain_coeff_3,three0.parms_id());
        //     p_evaluator->mod_switch_to_inplace(plain_coeff_4,three0.parms_id());

        //     p_evaluator->multiply_plain(three0,plain_coeff_3,three2);
        //     p_evaluator->rescale_to_next_inplace(three2);
        //     decrypt_check(three2,"three2");

        //     p_evaluator->multiply_plain(three0,plain_coeff_4,three1);
        //     p_evaluator->rescale_to_next_inplace(three1);
        //     p_evaluator->multiply(three0,three0,t2);
        //     p_evaluator->relinearize_inplace(t2,relin_keys);
        //     p_evaluator->rescale_to_next_inplace(t2);
        //     p_evaluator->multiply_inplace(three1,t2);
        //     p_evaluator->relinearize_inplace(three1,relin_keys);
        //     p_evaluator->rescale_to_next_inplace(three1);
        //     decrypt_check(three1,"three1");

        //     three1.scale()=scale;
        //     three2.scale()=scale;
        //     p_evaluator->mod_switch_to_inplace(three2,three1.parms_id());

        //     p_evaluator->add(three1,three2,three0);

        //   //  check_text(three0,"three0");
        //     decrypt_check(three0,"three0");
        // }
        // decrypt_result(three0,lable);

    }
       void lnx_cal( Ciphertext c_m,int num,int kind,int accuracy,double ln_num)
        {   
            plain_coeffs_ln133.resize(levs_ln133.size());

            int index=0;
            double devide=pow(10,num/2);
            if(num%2!=0)
            {
                devide*=5;
                p_encoder->encode(devide*ln_num,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=5;
            }
            for(int i=0;i<num/2;i++)
            {
                p_encoder->encode(devide*ln_num,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=2;
                p_encoder->encode(devide*ln_num,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=5;
            }
            p_encoder->encode(ln_num,scale,plain_coeffs_ln133[index]);
            //cout<<"devide="<<devide<<endl;
            cout<<"num="<<num<<endl;
            cout<<"plain_coeffs_ln133 size="<<plain_coeffs_ln133.size()<<endl;
            cout<<"index="<<index<<endl;
            coeff_p.resize(plain_coeffs.size());
            levs.resize(plain_coeffs.size());
            Plaintext coeff_1,coeff_ln133;
            Plaintext coeff_0;
            p_encoder->encode(0, scale, coeff_0);
            p_encoder->encode(1, scale, coeff_1);
           // p_encoder->encode(0.287682,scale,coeff_ln133);

            for(size_t i=0;i<plain_coeffs.size();i++)
            {
                
                p_encoder->encode(plain_coeffs[i],scale,coeff_p[i]);
            }
          
            //c_m泰勒展开
            cout<<"modulus_chain_index of c_m is:"<<p_context->get_context_data(c_m.parms_id())->chain_index()<<endl;
            cout<<"modulus_chain_index of coeff_1 is:"<<p_context->get_context_data(coeff_1.parms_id())->chain_index()<<endl;
           // p_evaluator->mod_switch_to_next_inplace(coeff_1);
            p_evaluator->sub_plain_inplace(c_m,coeff_1);//m=x-1

            Ciphertext mWith1;//m*1
            Ciphertext m2;//m*m
            Ciphertext suffix;//余项
          //  cout <<"tyler begin"<<endl;

           // p_evaluator->mod_switch_to_next_inplace(coeff_p[0]);
            p_evaluator->multiply_plain(c_m,coeff_p[0],levs[0]);
            p_evaluator->rescale_to_next_inplace(levs[0]);

          //  cout<<"levs[0] ready"<<endl;
if(accuracy>=2)
{
     p_evaluator->multiply_plain(c_m,coeff_1,mWith1);
           // p_evaluator->relinearize_inplace(mWith1,relin_keys);
            p_evaluator->rescale_to_next_inplace(mWith1);

            //p_evaluator->mod_switch_to_next_inplace(coeff_p[1]);
            p_evaluator->multiply_plain(c_m,coeff_p[1],levs[1]);
           // p_evaluator->relinearize_inplace(levs[1],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[1]);

            p_evaluator->multiply_inplace(levs[1],mWith1);
            p_evaluator->relinearize_inplace(levs[1],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[1]);
           // decrypt_check(levs[1],"levs[1]");
        //    cout<<"levs[1] ready"<<endl;
}
if(accuracy>=3)
{
    // p_evaluator->mod_switch_to_next_inplace(coeff_p[2]);
            p_evaluator->multiply_plain(c_m,coeff_p[2],levs[2]);
            p_evaluator->rescale_to_next_inplace(levs[2]);

            p_evaluator->square(c_m,m2);
            p_evaluator->relinearize_inplace(m2,relin_keys);
            p_evaluator->rescale_to_next_inplace(m2);

            p_evaluator->multiply_inplace(levs[2],m2);
            p_evaluator->relinearize_inplace(levs[2],relin_keys);
            p_evaluator->rescale_to_next_inplace(levs[2]);
           // decrypt_check(levs[2],"levs[2]");
         //   cout<<"levs[2] ready"<<endl;
}  

if(accuracy>=4)
{
    p_evaluator->multiply_plain(c_m,coeff_p[3],levs[3]);
    p_evaluator->rescale_to_next_inplace(levs[3]);

    p_evaluator->multiply_inplace(levs[3],m2);
    p_evaluator->relinearize_inplace(levs[3],relin_keys);
    p_evaluator->rescale_to_next_inplace(levs[3]);

    Ciphertext c_m_help=c_m;
    p_evaluator->mod_switch_to_inplace(c_m_help,levs[3].parms_id());

    p_evaluator->multiply_inplace(levs[3],c_m_help);
    p_evaluator->relinearize_inplace(levs[3],relin_keys);
    p_evaluator->rescale_to_next_inplace(levs[3]);

}    
if(accuracy>=5)
{
    p_evaluator->multiply_plain(c_m,coeff_p[4],levs[4]);
    p_evaluator->rescale_to_next_inplace(levs[4]);

    p_evaluator->multiply_inplace(levs[4],m2);
    p_evaluator->relinearize_inplace(levs[4],relin_keys);
    p_evaluator->rescale_to_next_inplace(levs[4]);

    Ciphertext m2_help=m2;
    p_evaluator->mod_switch_to_inplace(m2_help,levs[4].parms_id());

    p_evaluator->multiply_inplace(levs[4],m2_help);
    p_evaluator->relinearize_inplace(levs[4],relin_keys);
    p_evaluator->rescale_to_next_inplace(levs[4]);
}      

        //    levs[0].scale()=scale;
        //    levs[1].scale()=scale;
        //    levs[2].scale()=scale;
            for(int i=0;i<accuracy;i++)
            {
                levs[i].scale()=scale;
                if(levs[i].parms_id()!=levs[accuracy-1].parms_id())
                {
                    p_evaluator->mod_switch_to_inplace(levs[i],levs[accuracy-1].parms_id());
                }
            }

       //     p_evaluator->mod_switch_to_inplace(levs[0],levs[accuracy-1].parms_id());
     //       cout<<"modulus_chain_index of levs[0] is:"<<p_context->get_context_data(levs[0].parms_id())->chain_index()<<endl;
      //      cout<<"modulus_chain_index of levs[2] is:"<<p_context->get_context_data(levs[2].parms_id())->chain_index()<<endl;
      //      cout<<"modulus_chain_index of levs[1] is:"<<p_context->get_context_data(levs[1].parms_id())->chain_index()<<endl;
            Ciphertext c_r;
            for(int i=0;i<accuracy;i++)
            {
                cout<<"i="<<i<<endl;
                if(i==0&&accuracy==1)
                {

                c_r=levs[0];
                }
                else if(i==0)
                {
                    p_evaluator->add(levs[0],levs[1],c_r);
                    i++;
                }
                else
                {
                    p_evaluator->add_inplace(c_r,levs[i]);
                }
            }

          //  cout<<"c_r ready"<<endl;
            p_encryptor->encrypt(coeff_0,suffix);
            p_evaluator->mod_switch_to_inplace(suffix,levs[0].parms_id());
           
        //   check_text(levs[0],"levs[0]");
            for(size_t i=0;i<plain_coeffs_ln133.size();i++)
            { 
            cout<<levs_ln133.size()<<endl;
         // cout<<"modulus_chain_index of levs_ln133[i] is:"<<p_context->get_context_data(levs_ln133[i].parms_id())->chain_index()<<endl;
         // cout<<"modulus_chain_index of plain_coeffs_ln133[i] is:"<<p_context->get_context_data(plain_coeffs_ln133[i].parms_id())->chain_index()<<endl;
                p_evaluator->multiply_plain_inplace(levs_ln133[i],plain_coeffs_ln133[i]);

                p_evaluator->rescale_to_next_inplace(levs_ln133[i]);
                if(accuracy!=1)
                {
                    p_evaluator->mod_switch_to_inplace(levs_ln133[i],suffix.parms_id()); 
                }
                check_text(levs_ln133[i],"levs_ln133[i]");
                check_text(suffix,"suffix");
                levs_ln133[i].scale()=scale;
                p_evaluator->add_inplace(suffix,levs_ln133[i]);
               // decrypt_check(suffix,"suffix");
                            }

            cout<<"suffix calculated"<<endl;
            suffix.scale()=scale;
            p_evaluator->mod_switch_to_inplace(suffix,c_r.parms_id());

            decrypt_check(c_r,"c_r");
            p_evaluator->add_inplace(c_r,suffix); 
            decrypt_result(c_r,kind);  
        }


    template <typename T>
    void decrypt_check(T c,string name)
{
        Plaintext p3;
        if constexpr (is_same<T,Ciphertext>::value)
        {
 p_decryptor->decrypt(c,p3);
        }
        else if constexpr (is_same<T,Plaintext>::value)
        {
            p3=c;
        }
           
            vector<double > res3;
            p_encoder->decode(p3,res3);
            //string name=GET_NAME(c);
            cout<<name<<"="<<res3[0]<<endl;
}
template <typename T2> 
   void check_text( T2 t_r,string name)
   {
        cout<<"scale of "<<name<<" is:"<<t_r.scale()<<endl;
        cout<<"modulus chain index of "<<name<<" is:"<<p_context->get_context_data(t_r.parms_id())->chain_index()<<endl;
   }
 
    void lnx_compare(double x,int kind)
    {
        if(kind==2)
        {
            x=1.0/x;
        }
        double true_result=log(x);
        cout<<"true result is "<<true_result<<endl;
        double wucha=abs((true_result-result[0])/true_result);
        cout<<"相对误差："<<wucha*100<<"%"<<endl;
    }
 void   tri_compare(double x,int kind,int lable)
    {
        double true_result;
        double wucha; 
        true_result = cos(x);
        if(lable==2)
        {
            true_result = -true_result;
        }
        
            
 
        cout<<"true result is: "<<true_result<<endl;
            wucha =abs((true_result - result[0])/true_result);
            cout<<"相对误差："<<wucha*100<<"%"<<endl;
    }
void exp_compare(double x,int lable,int time)
{
    double true_result;
    double wucha;
    if(lable==1)
    {
        x=-x;
    }
    if(time>0)
    {
           result[0]=pow(result[0],pow(4,time));
    }
    cout<<"breaking time = "<<time<<endl;
    cout<<"actrual result estimated for exp is: "<<result[0]<<endl;
    true_result=exp(x*pow(4,time));
    cout<<"true result is: "<<true_result<<endl;
    wucha=abs((true_result - result[0])/true_result);
    cout<<"相对误差："<<wucha*100<<"%"<<endl;
}
};





 






