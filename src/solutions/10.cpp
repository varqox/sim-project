#include <iostream>
using namespace std;

main()

{

int a,b;

cout << " najwiekszy wspolny dzielnik " <<endl <<endl;
cout<<"a="; cin>>a;

cout << " b= "; cin>>b;
while (a!=b)
if (a<b) b=b-a;
else     a=a-b;


cout << " NWD wynosi: " << a << endl;
system ("pause");
}
