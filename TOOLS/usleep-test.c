
main(){
int u;

for(u=0;u<30000;u+=500){
    unsigned int x[9];
    int i;
    x[0]=GetTimer();
    for(i=1;i<=8;i++){
	usleep(u);
	x[i]=GetTimer();
    }
    printf("%d -> %d %d %d %d %d %d %d %d\n",u,
	x[1]-x[0],
	x[2]-x[1],
	x[3]-x[2],
	x[4]-x[3],
	x[5]-x[4],
	x[6]-x[5],
	x[7]-x[6],
	x[8]-x[7]
	);
}


}
