#include<iostream>
#include<vector>
#include<cmath>
//#include<string>
#include"seal/seal.h"

using namespace std;
using namespace seal;
//lnx calculation.coded bu xtx;
//注解：
//这份代码只是一个用于验证和演示的简单初版代码（写得很烂）。需要修改。
//该代码计算量可能无谓地偏大。
//该代码只在特定区间内精度较高。当x<0.6时，相对误差将大于10%；当x>10000000000时，绝对误差达到0.1
//该代码部分计算需要在非加密情况下进行（不符合全同态加密的要求，鉴于这只是示例练习代码，就没有那么扣细节。后期会修改）。
//该代码泰勒展开时，只保留了3项。
//后期会研究更新算法。

void lnx(double m,int i)
{
    EncryptionParameters parms(scheme_type::ckks);//选择加密方式为ckks
    size_t poly_modulus_degree=8192;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree,{60,40,40,60}));
    //选择初始缩放因子
    double scale=pow(2.0,40);
    //创建上下文对象
    SEALContext context(parms);
    //生成密钥生成器实体
    KeyGenerator keygen(context);
    //生成各种密钥
    auto secret_key=keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);
    GaloisKeys gal_keys;
    keygen.create_galois_keys(gal_keys);
    //创建加密器，评估器，解密器
    Encryptor encryptor(context,public_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context,secret_key);
    //创建用于编码解码的coder对象
    CKKSEncoder encoder(context);
    //创建明文
   Plaintext plain_1,plain_05,plain_033,plain_025,plain_ln2;
  encoder.encode(1,scale,plain_1);//1的明文
  encoder.encode(-0.5,scale,plain_05);//泰勒展开第二项的明文
  encoder.encode(0.33,scale,plain_033);//泰勒展开第三项的明文
  encoder.encode(-0.25,scale,plain_025);
  encoder.encode(0.69,scale,plain_ln2);//ln2的明文
    //加密输入的数值m和i
    Plaintext m_plain,i_plain;
    encoder.encode(m,scale,m_plain);
    encoder.encode(i,scale,i_plain);
    Ciphertext m_encrypted;
    Ciphertext i_encrypted;
    encryptor.encrypt(m_plain,m_encrypted);
    encryptor.encrypt(i_plain,i_encrypted);
  cout<<"m&i ready"<<endl;
    //rewrite
    cout<<"multiple begin"<<endl;
    Ciphertext input;
    evaluator.sub_plain(m_encrypted,plain_1,input);//input=m-1
    cout<<"x-1 ready"<<endl;
    Ciphertext lev2;//泰勒展开第二项
    Ciphertext lev3;//泰勒展开第三项
    Ciphertext lev4;//泰勒展开第四项（没写）
    Ciphertext Xwith1;//x*1(用于控制模数转换链索引)
    Ciphertext x2;//x的平方
    Ciphertext suffix;//后缀（非泰勒展开部分）
    Ciphertext result;//最终结果
    //接下来计算泰勒展开第二项
    //泰勒展开第二项为带系数的二次项，直接计算会导致模数转换链索引不匹配的问题，进而面临损失精度的风险。所以这里使用X*1,先计算x*1和x*-0.5（保证模数转换链索引相同），再相乘。
    evaluator.multiply_plain(input,plain_1,Xwith1);
    evaluator.rescale_to_next_inplace(Xwith1);

    evaluator.multiply_plain(input,plain_05,lev2);
    evaluator.rescale_to_next_inplace(lev2);

    evaluator.multiply_inplace(lev2,Xwith1);
    evaluator.relinearize_inplace(lev2,relin_keys);
    evaluator.rescale_to_next_inplace(lev2);
    cout<<"modelus_chain_index for lev2 is: "<<context.get_context_data(lev2.parms_id())->chain_index()<<endl;
    cout<<"modelus_chain_index for input is: "<<context.get_context_data(input.parms_id())->chain_index()<<endl;
    cout<<"lev2 ready"<<endl;
  //计算泰勒展开第三项。直接计算x^2和x*0.33，再相乘。
    evaluator.multiply_plain(input,plain_033,lev3);
    evaluator.rescale_to_next_inplace(lev3);
    evaluator.multiply(input,input,x2);
    evaluator.relinearize_inplace(x2,relin_keys);
    evaluator.rescale_to_next_inplace(x2);
    evaluator.multiply_inplace(lev3,x2);
    evaluator.relinearize_inplace(lev3,relin_keys);
    evaluator.rescale_to_next_inplace(lev3);
    cout<<"modelus_chain_index for lev3 is: "<<context.get_context_data(lev3.parms_id())->chain_index()<<endl;
    cout<<"lev3 ready"<<endl;
  cout<<"multiple over. addition bggin"<<endl;
  //统一scale.不然无法后续计算
    input.scale()=scale;
    lev2.scale()=scale;
    lev3.scale()=scale;
cout<<"scale initialized"<<endl;
//根据前面打印的模数转换链索引可以看到，input的模数转换链索引为3,lev2和lev3的模数转换链索引为1。这里把索引统一，不然无法计算
    evaluator.mod_switch_to_inplace(input,lev3.parms_id());
    evaluator.add(input,lev2,result);
    evaluator.add_inplace(result,lev3);
    //接下来计算非泰勒部分。和其他乘法一样。
    evaluator.multiply_plain(i_encrypted,plain_ln2,suffix);
    evaluator.relinearize_inplace(suffix,relin_keys);
    evaluator.rescale_to_next_inplace(suffix);
    //计算完之后，统一尺度和模数转换链索引
   suffix.scale()=scale;
    evaluator.mod_switch_to_inplace(suffix,result.parms_id());
    //最后把泰勒部分和非泰勒部分进行相加，得到结果。
    evaluator.add_inplace(result,suffix);
//解码
    cout<<"addition over. decryption begin"<<endl;
    Plaintext result_plain;
    decryptor.decrypt(result,result_plain);
    vector<double> result_double;//decode函数无法把解码结果储存在double中，所以需要创建一个vector数组来接受解码的结果
    encoder.decode(result_plain,result_double);
    //打印接过
    cout<<"the result estimated is:"<<result_double[0]<<endl;
    
}
int main()
{
cout<<"this is test lnx"<<endl;
double x;
cout<<"input x:"<<endl;
cin>>x;
cout<<"the real result is:"<<log(x)<<endl;
//计算：将x拆解为m*2^i,这部分计算暂时在非加密情况下进行
int i=0;
while(x>1.3)
{
    x/=2;
    i++;
}
cout<<"the i is:"<<i<<endl;
cout<<"the m is:"<<x<<endl;
lnx(x,i);

return 0;

}
