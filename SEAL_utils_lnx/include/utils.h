#include"seal/seal.h"
#include<string>
#include<iostream>
#include<vector>
#include<cmath>
 
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
        bool sign_approx(Ciphertext encrypted):正负判断函数，可以判断输入变量是否为正数，如果为正数则返回true,如果为0或者负数则返回false。输入值为密文encrypted.
        void lnx_cal(Ciphertext c_r,int kind):lnx计算函数，可以根据输入值近似计算并打印对应lnx的值。输入值为密文c_r和模式kind.
        void lnx_compare(double x):lnx计算结果比较函数，可以计算真实结果和相对误差，并打印。输入值为初始输入值x.
    

*/
class utils
{

   private:
    EncryptionParameters *p_parms;
    size_t poly_modulus_degree=8192;
    SEALContext * p_context ;//上下文指针
    KeyGenerator * p_keygen;//密钥生成器指针
    Encryptor * p_encryptor;//加密器指针
    Decryptor * p_decryptor;//解密器指针
    Evaluator * p_evaluator;//计算器指针
    CKKSEncoder * p_encoder;//编码器指针
    PublicKey public_key;//公钥
    RelinKeys relin_keys;//线性化密钥
    double scale;//缩放因子
    double sign_poly_coeff=0.763546;//比较大小参数
    const vector<int> lnx_poly_coeffs = {50,30,30,30,50};//系数模数
    Plaintext p_x;//初始输入值明文
    Ciphertext c_x;//初始输入值密文
    Plaintext p_r; //结果明文 
    vector<double> result;//最终结果

    //lnx计算相关。用法建lnx_cal函数。
    Ciphertext c_m;//拆分输入值密文 
    Ciphertext c_i;//拆分指数值密文
    Plaintext p_sign_poly_coeff;//比较大小明文
   

public:
    utils(int kind,double x)
   { 
    init(kind);
    encrypt_input(x,kind);
    if(kind==1||kind==2)
    {
        lnx_cal(c_x,kind);
        lnx_compare(x);
    }
    
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
       p_parms->set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, lnx_poly_coeffs)); 
       scale=pow(2.0,30);
    }
    p_context =  new SEALContext (*p_parms, true, seal::sec_level_type::tc128);
    p_keygen= new KeyGenerator(*p_context);
    auto secret_key=p_keygen->secret_key();
    p_keygen->create_public_key(public_key);
    p_keygen->create_relin_keys(relin_keys);
    p_evaluator=new Evaluator(*p_context);
    p_encryptor=new Encryptor(*p_context, public_key);
    p_decryptor=new Decryptor(*p_context, secret_key);
    p_encoder=new CKKSEncoder(*p_context);

    
    p_encoder->encode(sign_poly_coeff, scale, p_sign_poly_coeff);

   }
     void encrypt_input(double x,int kind)
        {
            if(kind==2)
            {
                x=1.0/x;
            }
            p_encoder->encode(x, scale, p_x);
            p_encryptor->encrypt(p_x, c_x);
        }
    void decrypt_result(Ciphertext c_r,int kind)
    {
        p_decryptor->decrypt(c_r,p_r);
        p_encoder->decode(p_r,result);
        if(kind==2)
        {
            result[0]=-result[0];
        }
       
            cout<<"result is "<<result[0]<<endl;
        
        
    }

       void lnx_cal( Ciphertext c_x,int kind)
        {
            Plaintext coeff_temp_0,coeff_1,coeff_p1,coeff_05,coeff_033,coeff_025,coeff_ln133,p_num_of_ln133;
            p_encoder->encode(1, scale, coeff_1);
            p_encoder->encode(1, scale, coeff_temp_0);
            p_encoder->encode(-0.492, scale, coeff_05);
            p_encoder->encode(0.235,scale,coeff_033);
            p_encoder->encode(-0.25,scale,coeff_025);
            p_encoder->encode(1,scale,coeff_p1);
            p_encoder->encode(0.287682,scale,coeff_ln133);

            //拆分法：将输入值拆分为：x=m*(4/3)^i;其中拆分输入值m属于0.9~1.2，拆分指数值i属于0~40。
            Ciphertext c_m;
            Ciphertext c_check;
            Plaintext p_pow_2;
            int index=40;
            bool overZero=false;
            while(index>-1)
            {
                p_encoder->encode(1.2*pow(4.0/3.0,index),scale,p_pow_2);
                p_evaluator->sub_plain(c_x,p_pow_2,c_check);
                //接下来检查c_check的正负
                overZero = sign_approx(c_check);
               
                if(overZero)
                {
                    cout<<"index ="<<index<<", over zero"<<endl;
                    break;
                }
                index-=1;
                
            }
            index+=1;
            cout<<"index is "<<index<<endl;
            p_encoder->encode(index,scale,p_num_of_ln133);//拆分出来的ln1.33数量
            p_encryptor->encrypt(p_num_of_ln133,c_i);
            cout<<"in1.33 number calculated"<<endl;

            p_encoder->encode(1.0/pow(4.0/3.0,index),scale,p_pow_2);
            p_evaluator->multiply_plain(c_x,p_pow_2,c_m);
            p_evaluator->relinearize_inplace(c_m,relin_keys);
            p_evaluator->rescale_to_next_inplace(c_m);

          //  decrypt_check(c_m,"c_m");
            c_m.scale()=scale;
          //  decrypt_check(c_m,"c_m");
            //c_m泰勒展开
            cout<<"modulus_chain_index of c_m is:"<<p_context->get_context_data(c_m.parms_id())->chain_index()<<endl;
            cout<<"modulus_chain_index of coeff_1 is:"<<p_context->get_context_data(coeff_1.parms_id())->chain_index()<<endl;
            p_evaluator->mod_switch_to_next_inplace(coeff_1);
            p_evaluator->sub_plain_inplace(c_m,coeff_1);//m=x-1

          //  decrypt_check(c_m,"c_m");
            Ciphertext lev1;//泰勒一次项
            Ciphertext lev2;//泰勒二次项
            Ciphertext lev3;//泰勒三次项
            Ciphertext mWith1;//m*1
            Ciphertext m2;//m*m
            Ciphertext suffix;//余项
            cout <<"tyler begin"<<endl;

            p_evaluator->mod_switch_to_next_inplace(coeff_p1);
            p_evaluator->multiply_plain(c_m,coeff_p1,lev1);
            p_evaluator->rescale_to_next_inplace(lev1);
            cout<<"lev1 ready"<<endl;

            p_evaluator->multiply_plain(c_m,coeff_1,mWith1);
           // p_evaluator->relinearize_inplace(mWith1,relin_keys);
            p_evaluator->rescale_to_next_inplace(mWith1);

            p_evaluator->mod_switch_to_next_inplace(coeff_05);
            p_evaluator->multiply_plain(c_m,coeff_05,lev2);
           // p_evaluator->relinearize_inplace(lev2,relin_keys);
            p_evaluator->rescale_to_next_inplace(lev2);

            p_evaluator->multiply_inplace(lev2,mWith1);
            p_evaluator->relinearize_inplace(lev2,relin_keys);
            p_evaluator->rescale_to_next_inplace(lev2);
            decrypt_check(lev2,"lev2");
            cout<<"lev2 ready"<<endl;
            p_evaluator->mod_switch_to_next_inplace(coeff_033);
            p_evaluator->multiply_plain(c_m,coeff_033,lev3);
            p_evaluator->rescale_to_next_inplace(lev3);

            p_evaluator->square(c_m,m2);
            p_evaluator->relinearize_inplace(m2,relin_keys);
            p_evaluator->rescale_to_next_inplace(m2);

            p_evaluator->multiply_inplace(lev3,m2);
            p_evaluator->relinearize_inplace(lev3,relin_keys);
            p_evaluator->rescale_to_next_inplace(lev3);
            decrypt_check(lev3,"lev3");
            cout<<"lev3 ready"<<endl;

            lev1.scale()=scale;
            lev2.scale()=scale;
            lev3.scale()=scale;

            p_evaluator->mod_switch_to_inplace(lev1,lev3.parms_id());
            cout<<"modulus_chain_index of lev1 is:"<<p_context->get_context_data(lev1.parms_id())->chain_index()<<endl;
            cout<<"modulus_chain_index of lev3 is:"<<p_context->get_context_data(lev3.parms_id())->chain_index()<<endl;
            cout<<"modulus_chain_index of lev2 is:"<<p_context->get_context_data(lev2.parms_id())->chain_index()<<endl;
            Ciphertext c_r;
            p_evaluator->add(lev2,lev3,c_r);
            p_evaluator->add_inplace(c_r,lev1);
            p_evaluator->mod_switch_to_inplace(coeff_ln133,c_i.parms_id());
            p_evaluator->multiply_plain(c_i,coeff_ln133,suffix);

            p_evaluator->rescale_to_next_inplace(suffix);

            suffix.scale()=scale;
            p_evaluator->mod_switch_to_inplace(suffix,c_r.parms_id());

           // decrypt_check(c_r,"c_r");

            p_evaluator->add_inplace(c_r,suffix); 
            
           // decrypt_check(suffix,"suffix");

            decrypt_result(c_r,kind);  
        }

void decrypt_check(Ciphertext c,string name)
{
        Plaintext p3;
            p_decryptor->decrypt(c,p3);
            vector<double > res3;
            p_encoder->decode(p3,res3);
            //string name=GET_NAME(c);
            cout<<name<<"="<<res3[0]<<endl;
}

   
    bool sign_approx(Ciphertext encrypted)
    {
        //多项式近似看正负：coeff_0*x+coeff_1*x^2//该方法已经弃用
        /*
        vector<double> coeffs={0.987688,-0.161562,0.0};
        Plaintext coeff_0,coeff_1,coeff_2,p_1;
        p_encoder->encode(coeffs[0],scale,coeff_0);
        p_encoder->encode(coeffs[1],scale,coeff_1);
        p_encoder->encode(coeffs[2],scale,coeff_2);
        p_encoder->encode(1,scale,p_1);

        Ciphertext temp;
        p_evaluator->multiply_plain(encrypted,coeff_0,temp);
        p_evaluator->relinearize_inplace(temp,relin_keys);
        p_evaluator->rescale_to_next_inplace(temp);//k1*x

        Ciphertext temp2,temp3;
        p_evaluator->multiply_plain(encrypted,coeff_1,temp2);
        p_evaluator->relinearize_inplace(temp2,relin_keys);
        p_evaluator->rescale_to_next_inplace(temp2);

        p_evaluator->multiply_plain(encrypted,p_1,temp3);
        p_evaluator->relinearize_inplace(temp3,relin_keys);
        p_evaluator->rescale_to_next_inplace(temp3);

        p_evaluator->multiply_inplace(temp2,temp3);
        p_evaluator->relinearize_inplace(temp2,relin_keys);
        p_evaluator->rescale_to_next_inplace(temp2);

        p_evaluator->mod_switch_to_inplace(temp,temp2.parms_id());
        p_evaluator->mod_switch_to_inplace(coeff_2,temp.parms_id());
        temp.scale()=pow(2.0,40);
        temp2.scale()=pow(2.0,40);
   
        p_evaluator->add_inplace(temp,temp2);
        p_evaluator->add_plain_inplace(temp,coeff_2);

        Plaintext p_result;
        p_decryptor->decrypt(temp,p_result);
        vector<double> result,result2;
        p_encoder->decode(p_result,result);
        cout<<"result is "<<result[0]<<endl;
        return result[0]>0;
        */
        //方法2：乘以一个系数，再解密看正负。

        p_evaluator->multiply_plain_inplace(encrypted,p_sign_poly_coeff);
        p_evaluator->relinearize_inplace(encrypted,relin_keys);
        p_evaluator->rescale_to_next_inplace(encrypted);

         Plaintext p_result;
        p_decryptor->decrypt(encrypted,p_result);
        vector<double> result,result2;
        p_encoder->decode(p_result,result);
        cout<<"result is "<<result[0]<<endl;
        return result[0]>0;

    }

    void lnx_compare(double x)
    {
        double true_result=log(x);
        cout<<"true result is "<<true_result<<endl;
        double wucha=abs((true_result-result[0])/true_result);
        cout<<"相对误差："<<wucha*100<<"%"<<endl;
    }


};





 






