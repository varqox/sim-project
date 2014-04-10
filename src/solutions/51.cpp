// Zadanie: Bar sałatkowy XXI OI - etap 1
// Autor: Krzysztof Małysa
// Złożoność: czasowa - O(n lg n)

#include <algorithm>
#include <iostream>
#include <cstdio>
#include <vector>
#include <set>

using namespace std;

struct compare_first
{
	bool operator()(const pair<int, int>& a, const pair<int, int>& b) const
	{return a.first<b.first;}
};

struct compare_second
{
	bool operator()(const pair<int, int>& a, const pair<int, int>& b) const
	{return a.second<b.second;}
};

namespace ctl
{
  template<class T>
  struct less
  {
      bool operator()(const T& x, const T& y) const
      {return x<y;}
  };
  // typedef int T;
  template<class T, class Compare = ctl::less<T> >
  class RB_tree
  {
  protected:
    enum RBT_color { Red=false, Black=true };

    struct node_base
    {
      typedef node_base* _ptr;
      typedef const node_base* _const_ptr;
      RBT_color color;
      _ptr up, left, right;

      node_base(RBT_color _c=Black, _ptr _u=NULL, _ptr _l=NULL, _ptr _r=NULL)
      : color(_c), up(_u), left(_l), right(_r)
      {}

      node_base(const node_base& $)
      : color($.color), up($.up), left($.left), right($.right)
      {}

      node_base& operator=(const node_base& $)
      {
        color=$->color;
        up=$->up;
        left=$->left;
        right=$->right;
      }

      virtual ~node_base(){}
    };

    typedef node_base* _Base_nptr;
    typedef const node_base* _const_Base_nptr;

    struct node : node_base
    {
      typedef node* _ptr;
      typedef const node* _const_ptr;
      T key;
      node(RBT_color _c=Black, _Base_nptr _u=NULL, _Base_nptr _l=NULL, _Base_nptr _r=NULL, const T& _k=T())
      : node_base(_c, _u, _l, _r), key(_k)
      {}

      node(_const_ptr $)
      : node_base($->color, $->up, $->left, $->right), key($->key)
      {}
    };

    typedef node* _nptr;
    typedef const node* _const_nptr;

    Compare _M_key_compare;
    _Base_nptr nil, root;
    size_t _M_size;

    void destruct(_Base_nptr x)
    {
      if(x->left!=nil)
        destruct(x->left);
      if(x->right!=nil)
        destruct(x->right);
      delete x;
    }

    void recursive_copy(_nptr destination, _const_nptr source, _const_Base_nptr source_nil)
    {
      if(source->left==source_nil)
        destination->left=nil;
      else
        recursive_copy(static_cast<_nptr>(destination->left=new node(static_cast<_const_nptr>(source->left))), static_cast<_const_nptr>(source->left), source_nil);
      if(source->right==source_nil)
        destination->right=nil;
      else
        recursive_copy(static_cast<_nptr>(destination->right=new node(static_cast<_const_nptr>(source->right))), static_cast<_const_nptr>(source->right), source_nil);
    }

    static _Base_nptr minimum(_Base_nptr x, _Base_nptr _nil)
    {
      while(x->left!=_nil)
        x=x->left;
      return x;
    }

    static _Base_nptr maximum(_Base_nptr x, _Base_nptr _nil)
    {
      while(x->right!=_nil)
        x=x->right;
      return x;
    }

    static _Base_nptr successor(_Base_nptr x, _Base_nptr _nil)
    {
      if(x->right!=_nil)
        return minimum(x->right, _nil);
      _Base_nptr y=x->up;
      while(y!=_nil && x==y->right)
      {
        x=y;
        y=y->up;
      }
      return y;
    }

    static _Base_nptr predecessor(_Base_nptr x, _Base_nptr _nil)
    {
      if(x->left!=_nil)
        return maximum(x->left, _nil);
      _Base_nptr y=x->up;
      while(y!=_nil && x==y->left)
      {
        x=y;
        y=y->up;
      }
      return y;
    }

    void left_rotate(_Base_nptr x)
    {
      _Base_nptr y = x->right, p = x->up;
      x->right = y->left;
      if(y->left != nil)
        y->left->up = x;
      y->up = p;
      if(p == nil)
        root = y;
      else if(x == p->left)
        p->left = y;
      else
        p->right = y;
      y->left = x;
      x->up = y;
    }

    void right_rotate(_Base_nptr x)
    {
      _Base_nptr y = x->left, p = x->up;
      x->left = y->right;
      if(y->right != nil)
        y->right->up = x;
      y->up = p;
      if(p == nil)
        root = y;
      else if(x == p->left)
        p->left = y;
      else
        p->right = y;
      y->right = x;
      x->up = y;
    }

    void transplant(_Base_nptr u, _Base_nptr v)
    {
      if(u->up == nil)
        root = v;
      else if(u == u->up->left)
        u->up->left=v;
      else
        u->up->right=v;
      v->up = u->up;
    }

  public:
    RB_tree(): nil(new node_base), root(nil), _M_size(0)
    {
      nil->up = nil->left = nil->right = nil;
    }

    RB_tree(const RB_tree& _)
    : nil(new node_base), root(nil), _M_size(_._M_size)
    {
      nil->up = nil->left = nil->right = nil;
      if(_M_size)
      {
        root=new node(static_cast<_nptr>(_.root));
        recursive_copy(static_cast<_nptr>(root), static_cast<_const_nptr>(_.root), _.nil);
      }
    }

    RB_tree& operator=(const RB_tree& _)
    {
      nil=new node_base;
      nil->up = nil->left = nil->right = nil;
      root=nil;
      _M_size=_._M_size;
      if(_M_size)
      {
        root=new node(static_cast<_nptr>(_.root));
        recursive_copy(static_cast<_nptr>(root), static_cast<_const_nptr>(_.root), _.nil);
      }
      return *this;
    }

    void inorder_walk(_Base_nptr x, std::string sp, std::string sn)
    {
    #ifdef WIN32
      static std::string cr="  ", cl="  ", cp="  ";
      static bool _$_=true;
      if(_$_)
      {
        _$_=false;
        cr[0]=218;cr[1]=196;
        cl[0]=192;cl[1]=196;
        cp[0]=179;
      }
    #else
      static std::string cr="|-", cl="|-", cp="| ";
    #endif
      std::string t;
      if(x != nil)
      {
        t=sp;
        if(sn==cr) t[t.size()-2] = ' ';
        inorder_walk(x->right, t+cp, cr);
        t=t.substr(0,sp.size()-2);
        std::cout << t << sn << (x->color ? "B:":"R:") << static_cast<_nptr>(x)->key << std::endl;
        t=sp;
        if(sn==cl) t[t.size()-2] = ' ';
        inorder_walk(x->left, t+cp, cl);
      }
    }

    void print()
    {
      std::cout << "{\n";
      inorder_walk(root, "", "");
      std::cout << "}" << std::endl;
    }

    virtual ~RB_tree()
    {
      if(_M_size)
        destruct(root);
      delete nil;
    }

    class iterator
    {
      friend class RB_tree;
      friend class const_iterator;
    public:
      typedef T value_type;
      typedef T* pointer;
      typedef T& reference;

    private:
      _Base_nptr _M_node, _M_nil;

    public:
      iterator(): _M_node(), _M_nil()
      {}

      iterator(_Base_nptr x, _Base_nptr n): _M_node(x), _M_nil(n)
      {}

      reference operator*() const
      {return static_cast<_nptr>(_M_node)->key;}

      pointer operator->() const
      {return &static_cast<_nptr>(_M_node)->key;}

      iterator& operator++()
      {
        _M_node=successor(_M_node, _M_nil);
        return *this;
      }

      iterator operator++(int)
      {
        iterator tmp=*this;
        _M_node=successor(_M_node, _M_nil);
        return tmp;
      }

      iterator& operator--()
      {
        _M_node=predecessor(_M_node, _M_nil);
        return *this;
      }

      iterator operator--(int)
      {
        iterator tmp=*this;
        _M_node=predecessor(_M_node, _M_nil);
        return tmp;
      }

      bool operator==(const iterator& x) const
      {return _M_node == x._M_node;}

      bool operator!=(const iterator& x) const
      {return _M_node != x._M_node;}
    };

    class const_iterator
    {
      friend class RB_tree;
      friend class iterator;
    public:
      typedef const T value_type;
      typedef const T* pointer;
      typedef const T& reference;

    private:
      _Base_nptr _M_node, _M_nil;

    public:
      const_iterator(): _M_node(), _M_nil()
      {}

      const_iterator(const iterator& x): _M_node(x._M_node), _M_nil(x._M_nil)
      {}

      const_iterator(_Base_nptr x, _Base_nptr n): _M_node(x), _M_nil(n)
      {}

      reference operator*() const
      {return static_cast<_nptr>(_M_node)->key;}

      pointer operator->() const
      {return &static_cast<_nptr>(_M_node)->key;}

      const_iterator& operator++()
      {
        _M_node=successor(_M_node, _M_nil);
        return *this;
      }

      const_iterator operator++(int)
      {
        const_iterator tmp=*this;
        _M_node=successor(_M_node, _M_nil);
        return tmp;
      }

      const_iterator& operator--()
      {
        _M_node=predecessor(_M_node, _M_nil);
        return *this;
      }

      const_iterator operator--(int)
      {
        const_iterator tmp=*this;
        _M_node=predecessor(_M_node, _M_nil);
        return tmp;
      }

      bool operator==(const const_iterator& x) const
      {return _M_node == x._M_node;}

      bool operator!=(const const_iterator& x) const
      {return _M_node != x._M_node;}
    };

    size_t size() const
    {return _M_size;}

    bool empty() const
    {return _M_size == 0;}

    const_iterator begin() const
    {return const_iterator(nil->right, nil);}

    const_iterator end() const
    {return const_iterator(nil, nil);}

    void swap(const RB_tree& $)
    {
      swap(nil, $.nil);
      swap(root, $.root);
      swap(_M_size, $._M_size);
    }

    void clear()
    {
      if(_M_size)
        destruct(root);
      root = nil->up = nil->right = nil->left = nil;
    }

    const_iterator find(const T& k)
    {
      _nptr x = static_cast<_nptr>(root);
      while(x != nil && k != x->key)
      {
        if(_M_key_compare(k, x->key))
          x = static_cast<_nptr>(x->left);
        else
          x = static_cast<_nptr>(x->right);
      }
      return const_iterator(x, nil);
    }

    const_iterator lower_bound(const T& k)
    {
      _Base_nptr x = root, y = nil;
      while(x != nil)
      {
        if(_M_key_compare(static_cast<_nptr>(x)->key, k))
          x = x->right;
        else
          y = x, x = static_cast<_nptr>(x->left);
      }
      return const_iterator(y, nil);
    }

    const_iterator upper_bound(const T& k)
    {
      _Base_nptr x = root, y = nil;
      while(x != nil)
      {
        if(_M_key_compare(k, static_cast<_nptr>(x)->key))
          y = x, x = x->left;
        else
          x = x->right;
      }
      return const_iterator(y, nil);
    }

    const_iterator insert(const T& k)
    {
      ++_M_size;
      _Base_nptr y, z = new node(Red, root, nil, nil, k), x;
      const_iterator result(z, nil);
      if(z->up == nil)
        root = z;
      else
      {
        while(true)
        {
          x=z->up;
          if(_M_key_compare(k, static_cast<_nptr>(x)->key))
          {
            if(x->left == nil)
            {
              x->left = z;
              break;
            }
            z->up = x->left;
          }
          else
          {
            if(x->right == nil)
            {
              x->right = z;
              break;
            }
            z->up = x->right;
          }
        }
        while(z->up->color == Red)
        {
          // x=z->up;
          if(z->up == z->up->up->left)
          {
            y=z->up->up->right;
            if(y->color == Red)
            {
              z->up->color = y->color = Black;
              z->up->up->color = Red;
              z = z->up->up;
              continue;
            }
            if(z == z->up->right)
            {
              z = z->up;
              left_rotate(z);
            }
            z->up->color = Black;
            z->up->up->color = Red;
            right_rotate(z->up->up);
            break;
          }
          else
          {
            y=z->up->up->left;
            if(y->color == Red)
            {
              z->up->color = y->color = Black;
              z->up->up->color = Red;
              z = z->up->up;
              continue;
            }
            if(z == z->up->left)
            {
              z = z->up;
              right_rotate(z);
            }
            z->up->color = Black;
            z->up->up->color = Red;
            left_rotate(z->up->up);
            break;
          }
        }
      }
      root->color = Black;
      nil->right=minimum(root, nil);
      nil->left=maximum(root, nil);
      return result;
    }

    void erase(const const_iterator& it)
    {
      --_M_size;
      _Base_nptr z = it._M_node, y = z, x;
      RBT_color y_original_color = y->color;
      if(z->left == nil)
      {
        x = z->right;
        transplant(z, z->right);
      }
      else if(z->right == nil)
      {
        x = z->left;
        transplant(z, z->left);
      }
      else
      {
        y = minimum(z->right, nil);
        y_original_color = y->color;
        x = y->right;
        if(y->up == z)
          x->up = y;
        else
        {
          transplant(y, y->right);
          y->right = z->right;
          y->right->up = y;
        }
        transplant(z, y);
        y->left = z->left;
        y->left->up = y;
        y->color = z->color;
      }
      if(y_original_color == Black)
      {
        while(x != root && x->color == Black)
        {
          if(x == x->up->left)
          {
            z = x->up->right;
            if(z->color == Red)
            {
              z->color = Black;
              x->up->color = Red;
              left_rotate(x->up);
              z = x->up->right;
            }
            if(z->left->color == Black && z->right->color == Black)
            {
              z->color = Red;
              x = x->up;
              continue;
            }
            if(z->right->color == Black)
            {
              z->left->color = Black;
              z->color = Red;
              right_rotate(z);
              z = x->up->right;
            }
            z->color = x->up->color;
            x->up->color = z->right->color = Black;
            left_rotate(x->up);
            x = root;
            break;
          }
          else
          {
            z = x->up->left;
            if(z->color == Red)
            {
              z->color = Black;
              x->up->color = Red;
              right_rotate(x->up);
              z = x->up->left;
            }
            if(z->left->color == Black && z->right->color == Black)
            {
              z->color = Red;
              x = x->up;
              continue;
            }
            if(z->left->color == Black)
            {
              z->right->color = Black;
              z->color = Red;
              left_rotate(z);
              z = x->up->left;
            }
            z->color = x->up->color;
            x->up->color = z->left->color = Black;
            right_rotate(x->up);
            x = root;
            break;
          }
        }
        x->color = Black;
      }
      nil->right=minimum(root, nil);
      nil->left=maximum(root, nil);
    }

    void erase(const_iterator first, const_iterator after_last)
    {
        while(first != after_last)
        {
        #ifdef DEBUG
            print();
        #endif
            erase(first++);
        }
    #ifdef DEBUG
        print();
    #endif
    }
  };
}

ostream& operator<<(ostream& os, const pair<int, int>& myp)
{return os << '(' << myp.first << ',' << myp.second << ')';}

template<class T, class Compare>
std::ostream& operator<<(std::ostream& os, const std::set<T, Compare>& x)
{
  os << "{";
  if(!x.empty())
  {
    os << *x.begin();
    for(typeof(x.begin()) i=++x.begin(); i!=x.end(); ++i)
      os << "," << *i;
  }
return os << "}";
}

template<class T, class Compare>
std::ostream& operator<<(std::ostream& os, const std::multiset<T, Compare>& x)
{
  os << "{";
  if(!x.empty())
  {
    os << *x.begin();
    for(typeof(x.begin()) i=++x.begin(); i!=x.end(); ++i)
      os << "," << *i;
  }
return os << "}";
}

template<class T, class Compare>
std::ostream& operator<<(std::ostream& os, const ctl::RB_tree<T, Compare>& x)
{
  os << "{";
  if(!x.empty())
  {
    os << *x.begin();
    for(typeof(x.begin()) i=++x.begin(); i!=x.end(); ++i)
      os << "," << *i;
  }
return os << "}";
}

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

int main()
{
	int n, y, out=0;
	scanf("%i", &n);
	vector<int> f(2*n+2, -2);
	vector<pair<int, int> > front(n), back(n); // front ->, back <-
	vector<char> in(n);
	char c=getchar();
	f[y=n+1]=-1;
	for(int i=0; i<n; ++i)
	{
		back[i].second=i;
		in[i]=c=getchar();
		if(c=='p')
		{
			--y;
			back[i].first=f[y-1]+2;
		}
		else
		{
			++y;
			back[i].first=n;
		}
		f[y]=i;
	}
	for(int i=0; i<=n; ++i)
		f[i]=n+1;
	f[y=n+1]=n;
	for(int i=n-1; i>=0; --i)
	{
		front[i].first=i;
		if(in[i]=='p')
		{
			--y;
			front[i].second=f[y-1]-2;
		}
		else
		{
			++y;
			front[i].second=-1;
		}
		f[y]=i;
	}
	sort(back.begin(), back.end(), compare_first());
	ctl::RB_tree<pair<int, int>, compare_second> my_set; // ends ale different
	for(int i=0, j=0; i<n; ++i)
	{
		if(front[i].first>front[i].second) continue;
		while(back[j].first<=front[i].first)
		{
			if(back[j].first<=back[j].second)
				my_set.insert(back[j]);
			++j;
		}
		ctl::RB_tree<pair<int, int>, compare_second>::const_iterator k=my_set.upper_bound(front[i]);
		--k;
		out=max(out, k->second-front[i].first+1);
	}
	printf("%i\n", out);
return 0;
}