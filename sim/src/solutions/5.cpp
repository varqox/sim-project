#include <iostream>
#include <fstream>

using namespace std;

int main()
{
    ios_base::sync_with_stdio(false);
    fstream file("k.txt", ios::out);
    if(file.good()) cout << "Du bist getrollen :P" << endl;
    else cout << "Cannot create file" << endl;
return 0;
}
