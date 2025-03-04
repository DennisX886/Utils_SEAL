#include"seal/seal.h"
#include<string>
#include<iostream>
#include<vector>
#include<cmath>
#include<fstream>
 
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
        void lnx_cal(Ciphertext c_m,Ciphertext c_i,int kind):lnx计算函数，可以根据输入值近似计算并打印对应lnx的值。输入值为密文c_m\c_i和模式kind.
        void lnx_compare(double x):lnx计算结果比较函数，可以计算真实结果和相对误差，并打印。输入值为初始输入值x.
        void encrypt_input_lnx(double m,vector<int>suffix_ln133):专门用于对数运算的加密函数，接受对数运算传入的参数。
        void choose_coeffs(int accuracy,int kind)：用于读取文件系数的函数。输入为精度和类型
        void check_text(t t_r,string name):用于检查模数转换链索引和缩放因子大小的函数。输入为明文或密文对象，以及他们的名字（用于打印显示）
    

*/
class utils
{

   private:
    EncryptionParameters *p_parms;
    size_t poly_modulus_degree=8192;
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
    const vector<int> poly_coeffs = {49,30,30,30,30,49};//系数模数
   // const vector<int> poly_coeffs_2 = {98,60,60,60,60,98};
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
    utils(int kind,double x)
   { 
    cout<<"utils 1 used"<<endl;
    
    }
    //重载构造函数2：专门为lnx设计
    utils(int kind,double x,double m,vector<int>suffix_ln133,int accuracy)
    {
        init(kind);
        choose_coeffs(accuracy,kind);
        encrypt_input_lnx(m,suffix_ln133);
        lnx_cal(c_m,suffix_ln133[0],kind,accuracy);
        lnx_compare(x,kind);

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
       p_parms->set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, poly_coeffs)); 
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
   } 

   void choose_coeffs(int accuracy,int kind)
    {
        string name;
        if(kind==1)
        {
            name="COEFFLIST_LNX";
        }
        fstream file(name);
        if(!file.is_open())
        {
            cerr<<"无法读取系数文件"<<endl;
            return ;
        }
        string line;
        double line_number=0;
        while(line_number<accuracy)
        {
            getline(file,line);
           line_number++;
        }
        if(line_number==accuracy)
        {
            istringstream iss(line);
            double num;
            while(iss>>num)
            {
                plain_coeffs.push_back(num);
            }
            if((int)plain_coeffs.size()==0)//这时，没有数据，需要自行创建写入
            {
                cout<<"系数为空，请检查系数文件"<<endl;
                return;
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
     void encrypt_input(double x,int kind)
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
       
            cout<<"result is "<<result[0]<<endl;
   
    }

       void lnx_cal( Ciphertext c_m,int num,int kind,int accuracy)
        {   
            plain_coeffs_ln133.resize(levs_ln133.size());
            double ln133=0.287432;//0.287682;//改为287432后精确度显著上升，推测是由于计算精度导致
            int index=0;
            double devide=pow(10,num/2);
            if(num%2!=0)
            {
                devide*=5;
                p_encoder->encode(devide*ln133,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=5;
            }
            for(int i=0;i<num/2;i++)
            {
                p_encoder->encode(devide*ln133,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=2;
                p_encoder->encode(devide*ln133,scale,plain_coeffs_ln133[index]);
                index++;
                devide/=5;
            }
            p_encoder->encode(ln133,scale,plain_coeffs_ln133[index]);
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
check_text(m2,"m2");
check_text(c_m,"c_m");
check_text(coeff_1,"coeff_1");
check_text(levs[2],"levs[2]");
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

void decrypt_check(Ciphertext c,string name)
{
        Plaintext p3;
            p_decryptor->decrypt(c,p3);
            vector<double > res3;
            p_encoder->decode(p3,res3);
            //string name=GET_NAME(c);
            cout<<name<<"="<<res3[0]<<endl;
}
    template <typename T>
   void check_text( T t_r,string name)
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


};





 






