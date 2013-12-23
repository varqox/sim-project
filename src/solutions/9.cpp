#include <cstring>
#include <stack>
// #include "unlint.hpp"
#ifndef __UNLINT_H
#define __UNLINT_H

#include <bits/localefwd.h>
#include <string>
#include <vector>

namespace unlimited_int
{
    /*
    unlimited integer type
    */
    class unlint
    {
    private:
        /*
        unlimited natural type
        */
        class num;
        bool z; // sign
        num* w; // value
    public:
        unlint();
        ~unlint();
        unlint(long long int);
        unlint(const char*);
        unlint(const std::string&);
        unlint(const unlint&);
        unlint& operator=(const unlint&);
        template<typename type>
        unlint& operator=(const type&);
        long long int size() const;
        void swap(unlint&);
        std::string str() const;
        unlint& operator++();
        unlint& operator--();
        unlint operator++(int);
        unlint operator--(int);
        friend unlint operator+(const unlint&);
        friend unlint operator-(const unlint&);
        unlint operator+(const unlint&);
        unlint& operator+=(const unlint&);
        unlint operator-(const unlint&);
        unlint& operator-=(const unlint&);
        unlint operator*(const unlint&);
        unlint& operator*=(const unlint&);
        unlint operator/(const unlint&);
        unlint& operator/=(const unlint&);
        unlint operator%(const unlint&);
        unlint& operator%=(const unlint&);
        bool operator>(const unlint&) const;
        bool operator<(const unlint&) const;
        bool operator>=(const unlint&) const;
        bool operator<=(const unlint&) const;
        bool operator==(const unlint&) const;
        bool operator!=(const unlint&) const;
        unlint& pow(const unlint&);
        unlint& factorial();
        friend unlint nwd(const unlint&, const unlint&);
        /* output unlint with ostream */
        friend std::ostream& operator<<(std::ostream&, const unlint&);
        /* input unlint with istream */
        friend std::istream& operator>>(std::istream&, unlint&);
    };

    unlint pow(const unlint&, const unlint&);
    unlint factorial(const unlint&);

    template<typename type>
    unlint& unlint::operator=(const type& _n)
    {
        unlint(_n).swap(*this);
    return *this;
    }
}

using namespace unlimited_int;

#endif /* __UNLINT_H */
#include <iostream>
//for FFT
#include <cmath>
#include <complex>

using namespace std;

namespace unlimited_int
{
    typedef long long int lli;
    const lli BASE=1000000000000000000LL, BS2=1000000000;
    const char LEN=18;

    class unlint::num
    {
    public:
        struct fmod
        {
            lli pom1, pom2;
            fmod(){}
            ~fmod(){}
        };
        std::vector<lli> w;
        num(): w(1,0){}
        ~num(){}
        num(const lli& _x): w(1,_x){}
        num(const num& _n): w(_n.w){}
        lli size() const;
        void kas0();
        void swap(num& _n){this->w.swap(_n.w);}
        num& operator++();
        num& operator--();
        num& operator+=(const num&);
        num& operator-=(const num&);
        num& operator*=(const lli&);
        void gen_mod(vector<fmod>&) const;
        num& mult(const lli&, const vector<fmod>&);
        void to_old_type(vector<int>&) const;
        num& from_old_type(vector<int>&);
        class FFT;
        num& operator*=(const num&);
        num& operator/=(const num&);
        num& operator%=(const num&);
        num& nwd(const num&);
        num& pow(const num&);
        bool operator<(const num&) const;
        bool operator>(const num&) const;
        bool operator<=(const num&) const;
        bool operator>=(const num&) const;
        bool operator==(const num&) const;
        bool operator!=(const num&) const;
    };

    lli unlint::num::size() const
    {
        lli w=(this->w.size()-1)*LEN, end=this->w[this->w.size()-1];
        if(end<1000000000LL)
        {
            if(end<10000LL)
            {
                if(end<100LL)
                {
                    if(end<10LL) ++w;
                    else w+=2;
                }
                else
                {
                    if(end<1000LL) w+=3;
                    else w+=4;
                }
            }
            else
            {
                if(end<1000000LL)
                {
                    if(end<100000LL) w+=5;
                    else w+=6;
                }
                else
                {
                    if(end<100000000LL)
                    {
                        if(end<10000000LL) w+=7;
                        else w+=8;
                    }
                    else w+=9;
                }
            }
        }
        else
        {
            if(end<10000000000000LL)
            {
                if(end<100000000000LL)
                {
                    if(end<10000000000LL) w+=10;
                    else w+=11;
                }
                else
                {
                    if(end<1000000000000LL) w+=12;
                    else w+=13;
                }
            }
            else
            {
                if(end<1000000000000000LL)
                {
                    if(end<100000000000000LL) w+=14;
                    else w+=15;
                }
                else
                {
                    if(end<100000000000000000LL)
                    {
                        if(end<10000000000000000LL) w+=16;
                        else w+=17;
                    }
                    else w+=18;
                }
            }
        }
    return w;
    }

    void unlint::num::kas0()
    {
        vector<lli>::iterator i=this->w.end()-1;
        while(i!=this->w.begin() && *i==0) --i;
        ++i;
        this->w.erase(i, this->w.end());
    }

    unlint::num& unlint::num::operator++()
    {
        vector<lli>::iterator i=this->w.begin();
        while(i!=this->w.end())
        {
            ++*i;
            if(*i<BASE) return *this;
            *i-=BASE;
            ++i;
        }
        this->w.push_back(1);
    return *this;
    }

    unlint::num& unlint::num::operator--()
    {
        vector<lli>::iterator i=this->w.begin();
        while(i!=this->w.end())
        {
            --*i;
            if(*i>=0) break;
            *i+=BASE;
            ++i;
        }
        this->kas0();
    return *this;
    }

    unlint::num& unlint::num::operator+=(const num& _n)
    {
        unsigned int s=_n.w.size(), i=0;
        if(s>this->w.size()) this->w.resize(s);
        bool add=false;
        for(; i<s; ++i)
        {
            this->w[i]+=_n.w[i];
            if(add) ++this->w[i];
            if(this->w[i]>=BASE)
            {
                this->w[i]-=BASE;
                add=true;
            }
            else add=false;
        }
        if(add)
        {
            if(i==this->w.size()) this->w.push_back(add);
            else
            {
                for(;i<this->w.size(); ++i)
                {
                    ++this->w[i];
                    if(this->w[i]<BASE) break;
                    this->w[i]-=BASE;
                }
                if(i==this->w.size()) this->w.push_back(add);
            }
        }
    return *this;
    }

    unlint::num& unlint::num::operator-=(const num& _n)
    {
        int s=_n.w.size(), i=0;
        bool add=false;
        for(; i<s; ++i)
        {
            this->w[i]-=_n.w[i];
            if(add) --this->w[i];
            if(this->w[i]<0)
            {
                this->w[i]+=BASE;
                add=true;
            }
            else add=false;
        }
        if(add)
        {
            s=this->w.size();
            for(;i<s; ++i)
            {
                --this->w[i];
                if(this->w[i]>=0) break;
                this->w[i]+=BASE;
            }
        }
        this->kas0();
    return *this;
    }

    unlint::num& unlint::num::operator*=(const lli& _lcb)
    {
        if(_lcb==0){vector<lli>(1).swap(this->w);return *this;}
        lli p1=_lcb/BS2, p2=_lcb-p1*BS2, add=0, pom1, pom2, pom3, add1;
        for(vector<lli>::iterator i=this->w.begin(); i!=this->w.end(); ++i)
        {
            pom1=*i/BS2;
            pom2=*i-pom1*BS2;
            *i=add+p2*pom2;
            add1=add=0;
            if(*i>=BASE){++add;*i-=BASE;}
            add1=pom1*p2+pom2*p1;
            pom3=add1/BS2;
            *i+=(add1-pom3*BS2)*BS2;
            while(*i>=BASE)
            {
                ++add;
                *i-=BASE;
            }
            add+=pom3+pom1*p1;
        }
        if(add) this->w.push_back(add);
    return *this;
    }

    void unlint::num::gen_mod(vector<num::fmod>& _k) const
    {
        int wl=this->w.size();
        _k.resize(wl);
        for(int i=0; i<wl; ++i)
        {
            _k[i].pom1=this->w[i]/BS2;
            _k[i].pom2=this->w[i]-_k[i].pom1*BS2;
        }
    }

    unlint::num& unlint::num::mult(const lli& _lcb, const vector<num::fmod>& _t)
    {
        if(_lcb==0){vector<lli>(1).swap(this->w);return *this;}
        int tl=_t.size();
        this->w.resize(tl);
        lli p1=_lcb/BS2, p2=_lcb-p1*BS2, add=0, pom3, add1;
        for(int i=0; i<tl; ++i)
        {
            this->w[i]=add+p2*_t[i].pom2;
            add1=add=0;
            if(this->w[i]>=BASE){++add;this->w[i]-=BASE;}
            add1=_t[i].pom1*p2+_t[i].pom2*p1;
            pom3=add1/BS2;
            this->w[i]+=(add1-pom3*BS2)*BS2;
            while(this->w[i]>=BASE)
            {
                ++add;
                this->w[i]-=BASE;
            }
            add+=pom3+_t[i].pom1*p1;
        }
        if(add) this->w.push_back(add);
    return *this;
    }

    void old_kas0(vector<int>& _n)
    {
        vector<int>::iterator i=_n.end()-1;
        while(i!=_n.begin() && *i==0) --i;
        ++i;
        _n.erase(i, _n.end());
    }

    void unlint::num::to_old_type(vector<int>& _n) const
    {
        int wl=this->w.size();
        _n.resize(wl<<1);
        for(int i=0; i<wl; ++i)
        {
            _n[(i<<1)+1]=this->w[i]/BS2;
            _n[(i<<1)]=this->w[i]-_n[(i<<1)+1]*BS2;
        }
        old_kas0(_n);
    }

    unlint::num& unlint::num::from_old_type(vector<int>& _n)
    {
        int nl=_n.size();
        this->w.resize((nl+1)>>1);
        for(int i=0; i<nl; i+=2)
            w[i>>1]=_n[i];
        for(int i=1; i<nl; i+=2)
            w[i>>1]+=_n[i]*BS2;
    return *this;
    }

    class unlint::num::FFT
    {
    public:
        static inline lli div_mod(lli &a, const lli& m)
        {lli tmp=a;a/=m;return tmp-m*a;}
        static const int FFT_BASE=10000;
        typedef double D;
        static std::complex<D> *w;
        static int d;
         
        static void omega(const int& n, bool t)
        {
            int to=n>>1;
            if(t) 
                for(int i=0; i<to; ++i)
                { 
                    w[i]=std::complex<D>(cos(2*M_PI*i/n),sin(2*M_PI*i/n));
                    w[i+to]=std::complex<D>(-w[i].real(),-w[i].imag());
                }
            else
                for(int i=0; i<to; ++i)
                { 
                    w[i]=std::complex<D>(cos(2*M_PI*i/n),sin(-2*M_PI*i/n));
                    w[i+to]=std::complex<D>(-w[i].real(),-w[i].imag());
                }
        }

        static void DFT(std::complex <D>* A, const int& size)
        {
            if(size==1) return;
            int to=size>>1;
            std::complex <D> *X = new std::complex <D> [to];
            std::complex <D> *Y = new std::complex <D> [to];
            for(int i=0, j=0; i<size; i+=2, j++)
            {
                X[j]=A[i];
                Y[j]=A[i+1];
            }
            //delete[] A;
            DFT(X, to);
            DFT(Y, to);
            //std::complex <D> *B = new std::complex <D> [size];
            int pot=unlint::num::FFT::d>>(31-__builtin_clz(size));
            for(int i=0; i<to; ++i)
            {
                std::complex <D> q=w[i*pot]*Y[i];
                A[i]=X[i]+q;
                A[i+to]=X[i]-q;
            }
            delete[] X;
            delete[] Y;
        }
        static void fft(num& a, const num& b)
        {
            std::complex <D> *l1, *l2;
            int d1, d2, t1, t2, FFT_base=4;
            t1=a.size();
            d1=t1/FFT_base;
            if(t1%FFT_base!=0) ++d1;
            t2=b.size();
            d2=t2/FFT_base;
            if(t2%FFT_base!=0) ++d2;
            d=__builtin_popcount(d1+d2)==1 ? d1+d2:1<<(32-__builtin_clz(d1+d2));

            l1 = new std::complex <D> [d];
            l2 = new std::complex <D> [d];
            w = new std::complex <D> [d];

            for(int i=0; i<d; ++i)
                l1[i]=l2[i]=std::complex <D> (0.0,0.0);
            {
                bool is_rest=false;
                unsigned int i=0, j=0;
                lli rest=0;
                for(; i<a.w.size()-1; ++i, ++j)
                {
                    lli k=a.w[i];
                    if(is_rest)
                    {
                        is_rest=false;
                        l1[j]=rest+100*div_mod(k, 100);
                        l1[++j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                    }
                    else
                    {
                        is_rest=true;
                        l1[j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                        l1[++j]=div_mod(k, FFT_BASE);
                        rest=k;
                    }
                }
                lli k=*--a.w.end();
                --j;
                if(is_rest) l1[++j]=rest+100*div_mod(k, 100);
                while(k>0)
                    l1[++j]=div_mod(k, FFT_BASE);
            }
            {
                bool is_rest=false;
                unsigned int i=0, j=0;
                lli rest=0;
                for(; i<b.w.size()-1; ++i, ++j)
                {
                    lli k=b.w[i];
                    if(is_rest)
                    {
                        is_rest=false;
                        l2[j]=rest+100*div_mod(k, 100);
                        l2[++j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                    }
                    else
                    {
                        is_rest=true;
                        l2[j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                        l2[++j]=div_mod(k, FFT_BASE);
                        rest=k;
                    }
                }
                lli k=*--b.w.end();
                --j;
                if(is_rest) l2[++j]=rest+100*div_mod(k, 100);
                while(k>0)
                    l2[++j]=div_mod(k, FFT_BASE);
            }
            omega(d, true);
            DFT(l1, d);
            DFT(l2, d);
            for(int i=0; i<d; ++i)
                l1[i]*=l2[i];
            delete[] l2;

            omega(d, false);
            DFT(l1,d);
            delete[] w;

            D s=1.0/d;
            a.w.resize(d*2/9+1);
            int j=0, idx=0;
            lli add=0, pow[9]={1LL, 10000LL, 100000000LL, 1000000000000LL, 10000000000000000LL, 100LL, 1000000LL, 10000000000LL, 100000000000000LL}, unpow[9]={1000000000000000000LL, 100000000000000LL, 10000000000LL, 1000000LL, 100LL, 10000000000000000LL, 1000000000000LL, 100000000LL, 10000LL};
            a.w[0]=0;
            for(int i=0; i<d; ++i, ++idx)
            {
                if(idx==5) 
                {
                    ++j;
                    a.w[j]=add;
                    add=0;
                }
                if(idx==9)
                {
                    ++j;
                    a.w[j]=add;
                    idx=add=0;
                }
                lli k=round(l1[i].real()*s);
                a.w[j]+=pow[idx]*div_mod(k,unpow[idx]);
                while(a.w[j]>=1000000000000000000LL)
                {
                    ++add;
                    a.w[j]-=1000000000000000000LL;
                }
                add+=k;
            }
            a.kas0();
            delete[] l1;
        }
    };

    int unlint::num::FFT::d;
    std::complex<unlint::num::FFT::D> *unlint::num::FFT::w;

    unlint::num& unlint::num::operator*=(const num& b)
    {
        if(b==1) return *this;
        FFT::fft(*this, b);
        /*num lol=0, _n;
        vector<num::fmod> t;
        b.gen_mod(t);
        for(unsigned int q=0; q<this->w.size(); ++q)
        {
            _n.mult(w[q], t);//k.w.insert(k.w.begin(),i,0);//lol+=k;
            unsigned int s=_n.w.size(), i=0;
            if(s+q>lol.w.size()) lol.w.resize(s+q);
            bool add=false;
            for(; i<s; ++i)
            {
                lol.w[i+q]+=_n.w[i];
                if(add) ++lol.w[i+q];
                if(lol.w[i+q]>=BASE)
                {
                    lol.w[i+q]-=BASE;
                    add=true;
                }
                else add=false;
            }
            if(add)
            {
                if(i==s) lol.w.push_back(add);
                else
                {
                    for(;i<s; ++i)
                    {
                        ++lol.w[i+q];
                        if(lol.w[i+q]<BASE) break;
                        lol.w[i+q]-=BASE;
                    }
                    if(i==s) lol.w.push_back(add);
                }
            }
        }
        this->swap(lol);
        this->kas0();*/
    return *this;
    }

    void div(vector<int>& a, vector<int>& b)
    {
        int al=a.size(), bl=b.size(), iws=al-bl;
        if(bl==1 && b[0]==1) return;
        else
        {
            bool is_grader;
            if(al<bl) is_grader=false;
            else if(al>bl) is_grader=true;
            else
            {
                int i=bl-1;
                while(i>=0 && a[i+iws]==b[i])
                    --i;
                if(i<0 || a[i+iws]>b[i]) is_grader=true;
                else is_grader=false;
            }
            if(!is_grader)
            {
                vector<int>(1,0).swap(a);
                return;
            }
        }
        vector<int> w(iws+1), g;
        while(iws>=0)
        {
            bool is_grader;
            if(al-iws<bl) is_grader=false;
            else if(al-iws>bl) is_grader=true;
            else
            {
                int i=bl-1;
                while(i>=0 && a[i+iws]==b[i])
                    --i;
                if(i<0 || a[i+iws]>b[i]) is_grader=true;
                else is_grader=false;
            }
            if(is_grader)
            {
                lli inter1=a[bl+iws-1], inter2=b[bl-1];
                if(al-iws>bl) inter1+=static_cast<lli>(BS2)*a[bl+iws];
                int down=std::max(1LL,inter1/(inter2+1)), up=std::min(BS2-1,(inter1+1)/inter2), mean;
                while(down<up)
                {
                    mean=1+((down+up)>>1);
                    //g=b*mean;
                    {
                        g.resize(bl);
                        int gl=bl;
                        lli tmp, add=0;
                        for (int i=0; i<gl; ++i)
                        {
                            tmp=static_cast<lli>(b[i])*mean+add;
                            add=tmp/BS2;
                            g[i]=tmp-add*BS2;
                        }
                        if(add>0) g.push_back(add);
                        old_kas0(g);
                    }
                    int gl=g.size();
                    if(al-iws<gl) is_grader=true;
                    else if(al-iws>gl) is_grader=false;
                    else
                    {
                        int i=gl-1;
                        while(i>=0 && a[i+iws]==g[i])
                            --i;
                        if(i<0) is_grader=false;
                        else if(g[i]>a[i+iws]) is_grader=true;
                        else is_grader=false;
                    }
                    if(is_grader) up=--mean;
                    else down=mean;
                }
                //g=b*down;
                {
                    g.resize(bl);
                    int gl=bl;
                    lli tmp, add=0;
                    for (int i=0; i<gl; ++i)
                    {
                        tmp=static_cast<lli>(b[i])*down+add;
                        add=tmp/BS2;
                        g[i]=tmp-add*BS2;
                    }
                    if(add>0) g.push_back(add);
                    old_kas0(g);
                }
                int gl=g.size();
                bool add=false;
                for(int i=0; i<gl; ++i)
                {
                    a[i+iws]-=g[i]+add;
                    if(a[i+iws]<0)
                    {
                        a[i+iws]+=BS2;
                        add=true;
                    }
                    else add=false;
                }
                for(int i=gl+iws; i<al; ++i)
                {
                    --a[i];
                    if(a[i]<0) a[i]+=BS2;
                    else break;
                }
                old_kas0(a);
                al=a.size();
                w[iws]=down;
            }
            --iws;
        }
        a.swap(w);
        old_kas0(a);
    }

    void mod(vector<int>& a, vector<int>& b)
    {
        int al=a.size(), bl=b.size(), iws=al-bl;
        if(bl==1 && b[0]==1)
        {
            vector<int>(1,0).swap(a);
            return;
        }
        else
        {
            bool is_grader;
            if(al<bl) is_grader=false;
            else if(al>bl) is_grader=true;
            else
            {
                int i=bl-1;
                while(i>=0 && a[i+iws]==b[i])
                    --i;
                if(i<0 || a[i+iws]>b[i]) is_grader=true;
                else is_grader=false;
            }
            if(!is_grader) return;
        }
        vector<int> g;
        while(iws>=0)
        {
            bool is_grader;
            if(al-iws<bl) is_grader=false;
            else if(al-iws>bl) is_grader=true;
            else
            {
                int i=bl-1;
                while(i>=0 && a[i+iws]==b[i])
                    --i;
                if(i<0 || a[i+iws]>b[i]) is_grader=true;
                else is_grader=false;
            }
            if(is_grader)
            {
                lli inter1=a[bl+iws-1], inter2=b[bl-1];
                if(al-iws>bl) inter1+=static_cast<lli>(BS2)*a[bl+iws];
                int down=std::max(1LL,inter1/(inter2+1)), up=std::min(BS2-1,(inter1+1)/inter2), mean;
                while(down<up)
                {
                    mean=1+((down+up)>>1);
                    //g=b*mean;
                    {
                        g.resize(bl);
                        int gl=bl;
                        lli tmp, add=0;
                        for (int i=0; i<gl; ++i)
                        {
                            tmp=static_cast<lli>(b[i])*mean+add;
                            add=tmp/BS2;
                            g[i]=tmp-add*BS2;
                        }
                        if(add>0) g.push_back(add);
                        old_kas0(g);
                    }
                    int gl=g.size();
                    if(al-iws<gl) is_grader=true;
                    else if(al-iws>gl) is_grader=false;
                    else
                    {
                        int i=gl-1;
                        while(i>=0 && a[i+iws]==g[i])
                            --i;
                        if(i<0) is_grader=false;
                        else if(g[i]>a[i+iws]) is_grader=true;
                        else is_grader=false;
                    }
                    if(is_grader) up=--mean;
                    else down=mean;
                }
                //g=b*down;
                {
                    g.resize(bl);
                    int gl=bl;
                    lli tmp, add=0;
                    for (int i=0; i<gl; ++i)
                    {
                        tmp=static_cast<lli>(b[i])*down+add;
                        add=tmp/BS2;
                        g[i]=tmp-add*BS2;
                    }
                    if(add>0) g.push_back(add);
                    old_kas0(g);
                }
                int gl=g.size();
                bool add=false;
                for(int i=0; i<gl; ++i)
                {
                    a[i+iws]-=g[i]+add;
                    if(a[i+iws]<0)
                    {
                        a[i+iws]+=BS2;
                        add=true;
                    }
                    else add=false;
                }
                for(int i=gl+iws; i<al; ++i)
                {
                    --a[i];
                    if(a[i]<0) a[i]+=BS2;
                    else break;
                }
                old_kas0(a);
                al=a.size();
            }
            --iws;
        }
        old_kas0(a);
    }

    unlint::num& unlint::num::operator/=(const num& _n)
    {
        vector<int> a,b;
        this->to_old_type(a);
        _n.to_old_type(b);
        div(a,b);
        this->from_old_type(a);
    return *this;
    }

    unlint::num& unlint::num::operator%=(const num& _n)
    {
        vector<int> a,b;
        this->to_old_type(a);
        _n.to_old_type(b);
        mod(a,b);
        this->from_old_type(a);
    return *this;
    }

    unlint::num& unlint::num::nwd(const num& _n)
    {
        vector<int> a, b, c;
        this->to_old_type(a);
        _n.to_old_type(b);
        while(!(b.size()==1 && b[0]==0))
        {
            c.swap(a);
            mod(c,b);
            a.swap(b);
            b.swap(c);
        }
        vector<int>().swap(b);
        vector<int>().swap(c);
        this->from_old_type(a);
    return *this;
    }

    unlint::num& unlint::num::pow(const num& _n)
    {
        if(_n.w.size()==1 && _n.w[0]==0)
        {
            vector<lli>(1,1).swap(this->w);
            return *this;
        }
        vector<lli> k(_n.w);
        stack<bool> bin;
        num pow1(*this);
        while(!(k.size()==1 && k[0]==1))
        {
            bin.push(!__builtin_ctz(k[0])); //last bit
            bool add=false;
            for(int i=k.size()-1; i>=0; --i)
            {
                if(add) k[i]+=BASE;
                if(!__builtin_ctz(k[i])) add=true; //if(__builtin_ctz(k[i])==0)
                else add=false;
                k[i]>>=1;
            }
            if(!k[k.size()-1]) k.pop_back(); //if(k[k.size()-1]==0)
        }
        while(!bin.empty())
        {
            this->operator*=(*this);
            if(bin.top()) this->operator*=(pow1);
            bin.pop();
        }
    return *this;
    }

    bool unlint::num::operator<(const num& _n) const
    {
        int i=this->w.size();
        if(static_cast<unsigned int>(i)<_n.w.size()) return true;
        else if(static_cast<unsigned int>(i)>_n.w.size()) return false;
        --i;
        while(i>=0 && this->w[i]==_n.w[i])
            --i;
        if(i<0) return false;
        if(this->w[i]>_n.w[i]) return false;
    return true;
    }

    bool unlint::num::operator>(const num& _n) const
    {
    return _n<*this;
    }

    bool unlint::num::operator<=(const num& _n) const
    {
    return !(_n<*this);
    }

    bool unlint::num::operator>=(const num& _n) const
    {
    return !this->operator<(_n);
    }

    bool unlint::num::operator==(const num& _n) const
    {
        int i=this->w.size();
        if(static_cast<unsigned int>(i)!=_n.w.size()) return false;
        --i;
        while(i>=0 && this->w[i]==_n.w[i])
            --i;
        if(i<0) return true;
    return false;
    }

    bool unlint::num::operator!=(const num& _n) const
    {
    return !this->operator==(_n);
    }

    string to_string(lli a)
    {
        stack<char> st;
        while(a>0)
        {
            st.push('0'+a%10);
            a/=10;
        }
        string w;
        while(!st.empty())
        {
            w+=st.top();
            st.pop();
        }
        if(w.empty()) w="0";
    return w;
    }
    /*---------------- UNLINT ----------------*/

    unlint::unlint(): w(new num)
    {}

    unlint::~unlint()
    {delete w;}

    unlint::unlint(lli k): z(true), w(new num)
    {
        if(k<0)
        {
            this->z=false;
            k=-k;
        }
        lli f=k/BASE;
        if(f>0) this->w->w.push_back(f);
        this->w->w[0]=k-f*BASE;
    }

    unlint::unlint(const char* cstr): z(true), w(new num)
    {
        int lenght=strlen(cstr), begin=0, idx=0;
        lli k;
        if(cstr[0]=='-'){z=false;begin=1;}
        this->w->w.resize(1+(lenght-begin)/LEN);
        for(int i=lenght-1; i>=begin; i-=LEN, ++idx)
        {
            k=0;
            for(int j=max(i-LEN+1,begin); j<=i; ++j)
            {
                k*=10;
                k+=cstr[j]-'0';
            }
            this->w->w[idx]=k;
        }
        this->w->kas0();
        if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
    }

    unlint::unlint(const string& str): z(true), w(new num)
    {
        int lenght=str.size(), begin=0, idx=0;
        lli k;
        if(str[0]=='-'){z=false;begin=1;}
        this->w->w.resize(1+(lenght-begin)/LEN);
        for(int i=lenght-1; i>=begin; i-=LEN, ++idx)
        {
            k=0;
            for(int j=max(i-LEN+1,begin); j<=i; ++j)
            {
                k*=10;
                k+=str[j]-'0';
            }
            this->w->w[idx]=k;
        }
        this->w->kas0();
        if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
    }

    unlint::unlint(const unlint& uli): z(uli.z), w(new num(*uli.w))
    {}

    unlint& unlint::operator=(const unlint& a)
    {
        unlint(a).swap(*this);
    return *this;
    }

    lli unlint::size() const
    {return this->w->size();}

    void unlint::swap(unlint& uli)
    {
        bool k;
        k=this->z;
        this->z=uli.z;
        uli.z=k;
        this->w->swap(*uli.w);
    }

    string unlint::str() const
    {
        lli k;
        bool begin=this->z ? false:true;
        string str(this->size()+begin, '0');
        if(begin) str[0]='-';
        for(int idx=0, j, i=str.size()-1; i>=begin; i-=LEN, ++idx)
        {
            j=i;
            k=this->w->w[idx];
            while(k>0)
            {
                str[j]+=k%10;
                k/=10;
                --j;
            }
        }
    return str;
    }

    unlint& unlint::operator++()
    {
        if(this->z) this->w->operator++();
        else this->w->operator--();
        if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
    return *this;
    }

    unlint& unlint::operator--()
    {
        if(this->w->w.size()==1 && this->w->w[0]==0)
        {
            this->z=false;
            this->w->w[0]=1;
        }
        else if(this->z) this->w->operator--();
        else this->w->operator++();
    return *this;
    }

    unlint unlint::operator++(int)
    {
        unlint k(*this);
        if(this->z) this->w->operator++();
        else this->w->operator--();
        if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
    return k;
    }

    unlint unlint::operator--(int)
    {
        unlint k(*this);
        if(this->w->w.size()==1 && this->w->w[0]==0)
        {
            this->z=false;
            this->w->w[0]=1;
        }
        else if(this->z) this->w->operator--();
        else this->w->operator++();
    return k;
    }

    unlint unlint::operator+(const unlint& _n)
    {
        unlint k(*this);
        if(k.z==_n.z) k.w->operator+=(*_n.w);
        else
        {
            if(k.w->operator>(*_n.w))
                k.w->operator-=(*_n.w);
            else
            {
                num emp(*_n.w);
                emp-=*k.w;
                k.w->swap(emp);
                if(k.w->w.size()==1 && k.w->w[0]==0) k.z=true;
                else k.z=!k.z;
            }
        }
    return k;
    }

    unlint& unlint::operator+=(const unlint& _n)
    {
        if(this->z==_n.z) this->w->operator+=(*_n.w);
        else
        {
            if(this->w->operator>(*_n.w))
                this->w->operator-=(*_n.w);
            else
            {
                num emp(*_n.w);
                emp-=*this->w;
                this->w->swap(emp);
                if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
                else this->z=!this->z;
            }
        }
    return *this;
    }

    unlint unlint::operator-(const unlint& _n)
    {
        unlint k(*this);
        if(k.z!=_n.z) k.w->operator+=(*_n.w);
        else
        {
            if(k.w->operator>(*_n.w))
                k.w->operator-=(*_n.w);
            else
            {
                num emp(*_n.w);
                emp-=*k.w;
                k.w->swap(emp);
                if(k.w->w.size()==1 && k.w->w[0]==0) k.z=true;
                else k.z=!k.z;
            }
        }
    return k;
    }

    unlint& unlint::operator-=(const unlint& _n)
    {
        if(this->z!=_n.z) this->w->operator+=(*_n.w);
        else
        {
            if(this->w->operator>(*_n.w))
                this->w->operator-=(*_n.w);
            else
            {
                num emp(*_n.w);
                emp-=*this->w;
                this->w->swap(emp);
                if(this->w->w.size()==1 && this->w->w[0]==0) this->z=true;
                else this->z=!this->z;
            }
        }
    return *this;
    }

    unlint unlint::operator*(const unlint& _n)
    {
        unlint k(*this);
        if(k.z==_n.z) k.z=true;
        else k.z=false;
        k.w->operator*=(*_n.w);
        if(*k.w==0) k.z=true;
    return k;
    }

    unlint& unlint::operator*=(const unlint& _n)
    {
        if(this->z==_n.z) this->z=true;
        else this->z=false;
        this->w->operator*=(*_n.w);
        if(*this->w==0) this->z=true;
    return *this;
    }

    unlint unlint::operator/(const unlint& _n)
    {
        unlint k(*this);
        if(k.z==_n.z) k.z=true;
        else k.z=false;
        k.w->operator/=(*_n.w);
        if(*k.w==0) k.z=true;
    return k;
    }

    unlint& unlint::operator/=(const unlint& _n)
    {
        if(this->z==_n.z) this->z=true;
        else this->z=false;
        this->w->operator/=(*_n.w);
        if(*this->w==0) this->z=true;
    return *this;
    }

    unlint unlint::operator%(const unlint& _n)
    {
        unlint k(*this);
        k.w->operator%=(*_n.w);
        if(!k.z && !(k.w->w.size()==1 && k.w->w[0]==0)) k+=(_n<0LL ? -_n:_n);
    return k;
    }

    unlint& unlint::operator%=(const unlint& _n)
    {
        this->w->operator%=(*_n.w);
        if(!this->z && !(this->w->w.size()==1 && this->w->w[0]==0)) this->operator+=(_n<0LL ? -_n:_n);
    return *this;
    }

    bool unlint::operator>(const unlint& _n) const
    {
        if(this->z!=_n.z) return this->z;
        if(this->z) return this->w->operator>(*_n.w);
    return this->w->operator<(*_n.w);
    }

    bool unlint::operator<(const unlint& _n) const
    {
        if(this->z!=_n.z) return _n.z;
        if(this->z) return this->w->operator<(*_n.w);
    return this->w->operator>(*_n.w);
    }

    bool unlint::operator>=(const unlint& _n) const
    {
        
        if(this->z!=_n.z) return this->z;
        if(this->z) return this->w->operator>=(*_n.w);
    return this->w->operator<=(*_n.w);
    }

    bool unlint::operator<=(const unlint& _n) const
    {
        if(this->z!=_n.z) return _n.z;
        if(this->z) return this->w->operator<=(*_n.w);
    return this->w->operator>=(*_n.w);
    }

    bool unlint::operator==(const unlint& _n) const
    {
        if(this->z==_n.z && this->w->operator==(*_n.w)) return true;
    return false;
    }

    bool unlint::operator!=(const unlint& _n) const
    {
        if(this->z==_n.z && this->w->operator==(*_n.w)) return false;
    return true;
    }

    unlint& unlint::pow(const unlint& _n)
    {
        if(_n.w->w.size()==1 && _n.w->w[0]==0)
        {
            this->z=true;
            vector<lli>(1,1).swap(this->w->w);
            return *this;
        }
        else if(this->w->w.size()==1 && this->w->w[0]==1)
        {
            if(!this->z && __builtin_ctz(_n.w->w[0])) this->z=true;
            return *this;
        }
        if(!this->z && __builtin_ctz(_n.w->w[0])) this->z=true;
        if(!_n.z)
        {
            this->z=true;
            vector<lli>(1,0).swap(this->w->w);
        }
        else this->w->pow(*_n.w);
    return *this;
    }

    unlint& unlint::factorial()
    {
        num mx(1), i(2);
        vector<num> lst;
        this->w->swap(mx);
        while(i<=mx)
        {
            lst.push_back(i);
            while(lst.size()>1 && (--lst.end())->w.size()>=(lst.end()-2)->w.size())
            {
                (lst.end()-2)->operator*=(*(--lst.end()));
                lst.pop_back();
            }
            ++i;
        }
        while(lst.size()>1)
        {
            (lst.end()-2)->operator*=(*(--lst.end()));
            lst.pop_back();
        }
        this->w->swap(lst[0]);
        this->z=true;
    return *this;
    }

    unlint operator+(const unlint& a)
    {return unlint(a);}

    unlint operator-(const unlint& a)
    {
        unlint k(a);
        k.z=!k.z;
    return k;
    }

    unlint nwd(const unlint& a, const unlint& b)
    {
        unlint w(a);
        w.w->nwd(*b.w);
        w.z=true;
    return w;
    }

    unlint pow(const unlint& a, const unlint& b)
    {
        unlint w(a);
        w.pow(b);
    return w;
    }

    unlint factorial(const unlint& a)
    {
        unlint w(a);
        w.factorial();
    return w;
    }

    /* output unlint with ostream */
    std::ostream& operator<<(std::ostream& os, const unlint& uli)
    {
        int ul=uli.w->w.size();
        if(!uli.z) os << '-';
        os << uli.w->w[--ul];
        for(int i=--ul; i>=0; --i)
        {
            os.width(LEN);
            os.fill('0');
            os << uli.w->w[i];
        }
    return os;
    }

    /* input unlint with istream */
    std::istream& operator>>(std::istream& is, unlint& uli)
    {
        std::string str;
        is >> str;
        uli=str;
    return is;
    }
}

int main()
{
    unlint a, b;
    cin >> a >> b;
    cout << nwd(a, b) << endl;
return 0;
}