#include<iostream>
#include<conio.h>
#include<cstdlib>
#include<cmath>
#include<ctime>
#include<cstring>
#include<windows.h>
using namespace std;
char p[9]; 
short k;
short tap,offset;
int score;
void gotoxy(int x, int y) {
    COORD coord; coord.X = x; coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}
int randomint(int min,int max){
	if(max==-47)return rand()%min;
	if(min>max)return rand()%(min-max)+max;
	return rand()%(max-min)+min;
}
void labinit(){
	for(int i=0;i<9;i++)p[i]=i+'1';
	for(int i=0;i<3;i++){
	    p[i]=p[i+6];
	    p[i+6]-=6;
    }
}
void graph(){
	gotoxy(0,0);
	for(int i=0;i<3;i++){
	    for(int j=0;j<3;j++)cout<<p[i*3+j]<<' ';
	    cout<<endl;
	}cout<<endl<<"Score: "<<score;
}
int main(){
	srand(time(0));
	labinit(); 
	while(1){
		tap=randomint(1,10);
		if(tap<4 and tap>0)offset=6;
		if(tap<10 and tap>6)offset=-6;
		for(int i=1;i<=randomint(10,31);i++)graph();
		p[tap-1+offset]='#';
		for(int i=1;i<=5;i++){
		    if(_kbhit()){
			    k=_getch();
			    if(k==27)break;
		        if(k==tap+'0'){
				    score++;
				    p[tap-1+offset]=tap+'0';
				    tap=0;
				}
		    }graph();
	    }labinit();
	}cout<<endl<<endl<<"bye!";
}
