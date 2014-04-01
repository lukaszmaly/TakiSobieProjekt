﻿/*
Znane błędy:
-jeśli karta jest idealnie równolegle(abs(a.y-d.y)~=0) to wystepuje problem z wyznaczeniem orientacji cardsInPlay
położenie problemu: metoda Card::Valid();
(dokonałem poprawki, ale nie mam na 100% pewnosci że problem zniknął, dlatego zostawiam ten punkt)
*/

#include <stdio.h>
#include <iostream>
#include "opencv2\imgproc\imgproc.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "settings.h"
#include "Card.h"
#include "Player.h"
#include "CardB.h"
#include "aruco.h"
#include <vector>
#include "marker.h"
#include "cvdrawingutils.h"
#include <fstream>
#include "Game.h"
#include "ScriptsManager.h"

using namespace aruco;
#define SAFEREGIONX 1100
#define SAFEREGIONY 150
#define SAFEREGIONWIDTH 230
#define SAFEREGIONHEIGHT 560
using namespace cv;

using namespace std;


int port=54000;

int c=0;
int three=200;
int maxArea= 31;
//int maxArea = 21;
int minArea=10;
//int minArea=10;
int zmienna=21;
int white=2800;
int gracz=0;
int faza=0;
int borderMethod = 0;
bool oneAttack=false;
int updateTime = 8;

void LoadCardDatabase(vector<CardB> &cards)
{
	cout<<"Loading Card`s database"<<endl;
	fstream plik("cards.txt", ios::in );
	int id,type,att,def,rCost,uCost,bCost,wCost,lCost,gCost,enterCount,passiveCount,upkeepCount;
	string name;
	bool hasHexproof, hasDeatchtuch,hasLifelink,hasDefender,hasFlying,hasReach,hasHaste,hasFirstStrike;
	vector<pair<int,int>> enterList,upkeepList,passiveList;

	while(!plik.eof())
	{
		plik>>id>>name>>type>>att>>def>>rCost>>wCost>>bCost>>uCost>>gCost>>lCost;
		plik>>hasDeatchtuch>>hasHexproof>>hasFirstStrike>>hasFlying>>hasReach>>hasDefender>>hasLifelink>>hasHaste;

		plik>>enterCount;
		for(unsigned int i=0;i<enterCount;i++)
		{
			int f,s;
			plik >> f >> s;
			enterList.push_back(make_pair(f,s));
		}

		plik>>upkeepCount;
		for(unsigned int i=0;i<upkeepCount;i++)
		{
			int f,s;
			plik >> f >> s;
			upkeepList.push_back(make_pair(f,s));
		}

		plik>>passiveCount;
		for(unsigned int i=0;i<passiveCount;i++)
		{
			int f,s;
			plik >> f >> s;
			passiveList.push_back(make_pair(f,s));
		}

		Mat a=imread("C:/Users/lukaszmaly/Documents/Visual Studio 2010/Projects/TakiSobieProjekt/TakiSobieProjekt/"+name+".jpg");
		if(!a.data) {cout<<"File "<< name<<".jpg not found"<<endl; continue;}
		cards.push_back(CardB(a));
		cards[cards.size()-1].Init(id,name,type,att,def,rCost,wCost,gCost,bCost,uCost,lCost,hasDefender,hasLifelink,hasDeatchtuch,hasHaste,hasFlying,hasReach,hasFirstStrike,hasHexproof,enterList,upkeepList,passiveList);
	}
	cout<<"Loaded "<< cards.size()<<" cards"<<endl;
}

bool IsInRectFast(Point p,Point a = Point(SAFEREGIONX,SAFEREGIONY) ,Point c = Point(SAFEREGIONX+SAFEREGIONWIDTH,SAFEREGIONY+SAFEREGIONHEIGHT))
{
	if(p.x<a.x || p.x>c.x || p.y<a.y || p.y>c.y) return false;
	return true;
}

void DrawSafeRegion(Mat &img)
{
	rectangle(img,Point(SAFEREGIONX,SAFEREGIONY),Point(SAFEREGIONX+SAFEREGIONWIDTH,SAFEREGIONHEIGHT),Scalar(0,0,255));
}


double angle( cv::Point pt1, cv::Point pt2, cv::Point pt0 ) {
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1*dy2)/sqrt((dx1*dx1 + dy1*dy1)*(dx2*dx2 + dy2*dy2));
}

vector<vector<Point>>FindSquaresInImage(Mat &image)
{
	Mat diff;
	vector<vector<Point> > contours;
	vector<vector<Point> > edge_pts;
	vector<Card> tcard;
	vector<vector<Point>> squares;
	vector<Point> approx;
	vector<Vec4i> hierarchy;		
	cvtColor(image,diff,CV_RGB2GRAY);
	Canny(diff,diff,three,three);
	dilate(diff, diff, Mat(), Point(-1,-1));
	findContours( diff, contours,hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

	for(unsigned int i=0;i<contours.size();i++)
	{
		if(contours[i].size()>5) edge_pts.push_back(contours[i]);
	}

	vector<vector<Point> >hull( edge_pts.size() );

	for(unsigned int i=0; i<edge_pts.size();i++)
	{  
		convexHull(Mat(edge_pts[i]),hull[i],true); 
	}


	for (size_t i = 0; i < edge_pts.size(); i++)
	{
		approxPolyDP(Mat(edge_pts[i]), approx, arcLength(Mat(edge_pts[i]), true)*0.02, true);
		if (approx.size() == 4 && fabs(contourArea(Mat(approx)))>minArea*1000 && fabs(contourArea(Mat(approx)))<maxArea*1000)
		{

			double maxCosine = 0;
			for (int j=2; j<5; j++)
			{
				double cosine = fabs(angle(approx[j%4], approx[j-2], approx[j-1]));
				maxCosine = MAX(maxCosine, cosine);
			}
			//if(maxCosine>=zmienna/(double)10)
			squares.push_back(approx);
		}
	}
	image=diff;
	return squares;
}

bool IsTheProperlyCard(Card &temp,vector<Card> &cards)
{
	Point2f center = temp.getCenter();
	for(unsigned int i=0;i<cards.size();i++)
	{
		if(Game::Distance(center,cards[i].getCenter())<20) return false;
	}
	return true;
}

void MainCardLogic(Mat &frame,vector<Card> &cards,vector<Card>&stack,vector<CardB> &bcards,Game &game)
{
	Mat diff;
	vector<vector<Point>> squares;
	frame.copyTo(diff);
	squares=FindSquaresInImage(diff);
	vector<Card> tcard;
	//Poprawa rogów kart
	for(unsigned int i=0;i<squares.size();i++)
	{
		Card::Prepare(squares[i],frame);
		drawContours(diff,squares,i,Scalar(255,255,255),2);
	}

	imshow("Kontury",diff);
	int erased =0;
	for(unsigned int i=0;i<squares.size();i++)
	{	
		if(IsInRectFast(Card::getCenter(squares[i][0],squares[i][1],squares[i][2],squares[i][3]),game.firsCardPoint,game.firsCardPoint+Point(game.firstCardWidth,game.firstCardHeight))==true)
		{
			continue;
		}
		if(game.CheckCardsProp()==true && Card::Valid(squares[i][0],squares[i][1],squares[i][2],squares[i][3])==false)
		{
			squares.erase(squares.begin()+i);
			i=-1;
			erased++;
		}
	}
	if(erased!=0) cout<<erased <<" kart nie spelnialo norm"<<endl;
	
	//tworzenie tymczaswego wektora z wykrytymi kartami na stole
	for (unsigned int i = 0; i<squares.size(); i++ ) 
	{
		tcard.push_back(Card(squares[i][0],squares[i][1],squares[i][2],squares[i][3],frame,bcards,game,true));
	}
	//	
	//redukcja powtórzeń
	for (unsigned int i = 0; i<tcard.size(); i++) 
	{
		for(unsigned int j=0; j<tcard.size(); j++)
		{
			if(i!=j && Game::Distance(tcard[i].getCenter(),tcard[j].getCenter())<40)
			{
				tcard.erase(tcard.begin()+j);
				i=-1;
				j=-1;
				break;
			}
		}
	}
	//
	if(game.firstCardChecked==false)
	{
		rectangle(frame,game.firsCardPoint,game.firsCardPoint+Point(game.firstCardWidth,game.firstCardHeight),Scalar(0,0,255),4);
		for(unsigned int i=0;i<tcard.size();i++)
		{
			if(IsInRectFast(tcard[i].getCenter(),game.firsCardPoint,game.firsCardPoint+Point(game.firstCardWidth,game.firstCardHeight))==true)
			{
				int w = (int)Card::Distance(tcard[i].a,tcard[i].b);
				int h = (int)Card::Distance(tcard[i].b,tcard[i].c);
				game.firstCardChecked=true;
				game.server.Markers(w,h);
				break;
				return;
			}
		}
		return;
	}

	///Sprawdzanie czy są cards na stackie
	///BLAD NAPRAW GO .BLAD WAZNY
	///mozna naprawić poprzez poprawienie warunku. nie szukaj najblizszego sasiada, tylko porównaj odległości między środkami
	for (unsigned int i = 0; i<stack.size(); i++ ) 
	{
		int index=-1;
		int minn=90000;
		for(unsigned int j=0;j<tcard.size();j++)
		{
			if(IsInRectFast(tcard[j].getCenter())==true)//&& stack[i].cardBase.id==tcard[j].cardBase.id
			{
				if(Game::Distance(stack[i].getCenter(),tcard[j].getCenter())<minn)
				{
					minn=Game::Distance(stack[i].getCenter(),tcard[j].getCenter());
					index=j;
				}
			}
		}
		if(index!=-1)
		{
			stack[i].Update(tcard[index].a,tcard[index].b,tcard[index].c,tcard[index].d,frame,bcards,game,false);
			tcard.erase(tcard.begin()+index);
		}
	}

	//dodanie nowych kart do stacku
	for(unsigned int i=0;i<tcard.size();i++)
	{
		if(IsInRectFast(tcard[i].getCenter())==true) 
		{
			tcard[i].owner=game.getCurrentPlayer();
			stack.push_back(tcard[i]);
			tcard.erase(tcard.begin()+i);
			i=-1;
		}
	}

	///redukcja duplikatów --jeśli błąd nie będzie się pojawiał można usunąć
	for (unsigned int i = 0; i<stack.size(); i++ ) 
	{
		for(unsigned int j=0;j<stack.size();j++)
		{
			if(i!=j && Game::Distance(stack[i].getCenter(),stack[j].getCenter())<30)
			{
				cout<<"BŁĄD: Usunieto  duplikat karty na stosie!"<<endl;
				game.server.Write("BLAD1");
				stack.erase(stack.begin()+max(j,i));
				j=-1;
				i=-1;
				break;
			}
		}
	}

	///Tutaj mamy pewność że w bezpiecznej strefie nie znajdują się cards ze stacku

	for (unsigned int i = 0; i<cards.size(); i++ ) 
	{
		int index=-1;
		int minn=90000;
		int tempDistance;
		for(unsigned int j=0;j<tcard.size();j++)
		{
			if(cards[i].cardBase.id==tcard[j].cardBase.id)
			{
				tempDistance=Game::Distance(cards[i].getCenter(),tcard[j].getCenter());

				if(tempDistance<20 ||(tempDistance<minn && IsTheProperlyCard(tcard[j],cards)==true))
				{
					minn=tempDistance;
					index=j;
				}
			}
		}
		if(index!=-1)
		{
			cards[i].Update(tcard[index].a,tcard[index].b,tcard[index].c,tcard[index].d,frame,bcards,game,false);
			tcard.erase(tcard.begin()+index);	
		}
	}

	for(unsigned int i=0;i<stack.size();i++)
	{
		if(stack[i].ttl<=0) 
		{
			cout<<"Przestarzala karta na stosie"<<endl;
			stack.erase(stack.begin()+i);
			i=-1;
		}
	}

	//dodanie nowych kart, które mogły się pojawić
	//dodają się tylko landy
	for(unsigned int i=0;i<tcard.size();i++)
	{
		if(tcard[i].cardBase.type==LAND)
		{
			tcard[i].Unlock();
			cards.push_back(tcard[i]);
			cout<<"Dodaje karte"<<endl;
			game.server.SendNewCard(tcard[i].id,tcard[i].cardBase.id,game.GetPlayer(tcard[i].owner),tcard[i].a,tcard[i].b,tcard[i].c,tcard[i].d,tcard[i].taped);
			tcard.erase(tcard.begin()+i);
			i=-1;
		}
	}

	for(unsigned int i=0;i<stack.size();i++)
	{
		for(unsigned int j=0;j<tcard.size();j++)
		{

			if(stack[i].cardBase.id==tcard[j].cardBase.id && IsInRectFast(tcard[j].getCenter())==false && stack[i].owner.mana.CanPay(stack[i].cardBase.whiteCost,stack[i].cardBase.blueCost,stack[i].cardBase.blackCost,stack[i].cardBase.redCost,stack[i].cardBase.greenCost,stack[i].cardBase.lessCost)) 
			{
		
				stack[i].owner.mana.Pay(stack[i].cardBase.whiteCost,stack[i].cardBase.blueCost,stack[i].cardBase.blackCost,stack[i].cardBase.redCost,stack[i].cardBase.greenCost,stack[i].cardBase.lessCost);
				game.server.SubMana(game.GetPlayer(stack[i].owner),stack[i].cardBase.whiteCost,stack[i].cardBase.blueCost,stack[i].cardBase.blackCost,stack[i].cardBase.redCost,stack[i].cardBase.greenCost);

				tcard[j].Unlock();
				stack[i].id=tcard[j].id;
				game.server.SendNewCard(tcard[j].id,tcard[j].cardBase.id,game.GetPlayer(tcard[j].owner),tcard[j].a,tcard[j].b,tcard[j].c,tcard[j].d,tcard[j].taped);
				cards.push_back(stack[i]);
				stack.erase(stack.begin()+i);
				tcard.erase(tcard.begin()+j);
				game.server.StackColor(1,NEUTRAL);
				i=j=-1;
				break;
			}
		}
	}

	///redukcja duplikatów -- mozna usunac jesli blad nie bedzie sie pojawial
	for (unsigned int i = 0; i<cards.size(); i++ ) 
	{
		for(unsigned int j=0;j<cards.size();j++)
		{
			if(i!=j && Game::Distance(cards[i].getCenter(),cards[j].getCenter())<30 && cards[i].cardBase.id == cards[j].cardBase.id)
			{
				cout<<"BLAD:USUWAM DUPLIKAT"<<endl;
				game.server.Write("BLAD2");
				cards.erase(cards.begin()+max(j,i));
				j=-1;
				i=-1;
				break;
			}
		}
	}

	for(unsigned int i=0;i<cards.size();i++)
	{
		if(!cards[i].TrySend(game)) continue;
		if(cards[i].attack==true)
		{
			game.server.Attack(cards[i].id,cards[i].cardBase.id,game.GetPlayer(cards[i].owner),cards[i].a,cards[i].b,cards[i].c,cards[i].d,cards[i].taped,cards[i].GetAttack(),cards[i].GetDefense());
		}
		else if(cards[i].block == true)
		{
			game.server.Block(cards[i].id,cards[i].cardBase.id,game.GetPlayer(cards[i].owner),cards[i].a,cards[i].b,cards[i].c,cards[i].d,cards[i].taped,cards[i].blocking,cards[i].GetAttack(),cards[i].GetDefense());
		}
		else
		{
			game.server.UpdateCard(cards[i].id,cards[i].cardBase.id,game.GetPlayer(cards[i].owner),cards[i].a,cards[i].b,cards[i].c,cards[i].d,cards[i].taped,cards[i].GetAttack(),cards[i].GetDefense());
		}
	}
	//WYSYLANIE Kosztu karty/lepiej zastapic oblsuga stosu(wymaga bazy kart)
	for(unsigned int i=0;i<stack.size();i++)
	{
		if(!stack[i].TrySend(game) || stack[i].cardBase.type==LAND) continue;
		if(stack[i].owner.mana.CanPay(stack[i].cardBase.whiteCost,stack[i].cardBase.blueCost,stack[i].cardBase.blackCost,stack[i].cardBase.redCost,stack[i].cardBase.greenCost,stack[i].cardBase.lessCost)==true)
		{
			game.server.StackColor(1,OK);
		}
		else
		{
			game.server.StackColor(1,DENY);
		}
	}
}

void MainGameLogic(Mat &frame,vector<Card> &cards,vector<Card>&stack,vector<CardB> &bcards,Game &game)
{
	MainCardLogic(frame,cards,stack,bcards,game);
	if(game.GetPhase()==ATAK)
	{
		for (unsigned int i = 0; i<cards.size(); i++ ) 
		{
			cards[i].attack=false; 
			if(cards[i].owner==game.getCurrentPlayer() && Game::Distance(cards[i].getCenter(),cards[i].old)>50 && cards[i].taped==true) 
			{
				cards[i].attack=true; 
			}
		}
	}
	else if(game.GetPhase()==OBRONA)
	{
		for (unsigned int i = 0; i<cards.size(); i++ ) 
		{
			cards[i].block=false; 
			if(!(cards[i].owner==game.getCurrentPlayer()) && Game::Distance(cards[i].getCenter(),cards[i].old)>50) 
			{
				cards[i].block=true; 
			}

		}
		for (unsigned int i = 0; i<cards.size(); i++ ) //cards broniace
		{	
			int index=-1;
			int minn=10000;
			int tempDistance;
			if(cards[i].block==false) continue;

			for(unsigned int j=0;j<cards.size();j++)//cards atakujace
			{
				if(cards[j].attack==false || i==j) continue;
				tempDistance=Game::Distance(cards[j].getCenter(),cards[i].getCenter());
				if(tempDistance<minn && cards[i].CanBlock(cards[j]))
				{
					minn=tempDistance;
					index=j;
				}
			}
			if(index!=-1)
			{
				cards[i].enemy=cards[index].getCenter();
				cards[i].blocking=cards[index].id;
			}
		}
	}
	else if(game.GetPhase()==WYMIANA && game.oneAttack==false)
	{
		cout<<"Wymiana obrazen"<<endl;
		game.oneAttack=true;
		for(int i=0;i<cards.size();i++)
		{
			if(cards[i].block==false) continue;
			for(int j=0;j<cards.size();j++)
			{
				if(cards[j].attack==false || i==j) continue;
				cards[i].Fight(cards[j],game);
				cout<<"WALKA"<<endl;
			}
		}

		for(int i=0;i<cards.size();i++)
		{
			if(cards[i].attack==true)
			{
				if(cards[i].owner==game.player1)
				{
					game.player2.hp-=cards[i].att;
					game.server.SubLife(2,cards[i].att);
				}
				else
				{
					game.player1.hp-=cards[i].att;
					game.server.SubLife(1,cards[i].att);
				}
			}
			cards[i].Clear();
		}
		game.nextPhase();
		//setTrackbarPos("Faza gry ","Ustawienia",4);
	}

	else if(game.GetPhase()==UPKEEP)
	{
		for(int i=0;i<cards.size();i++)
		{
			cards[i].NewRound();
		}
		setTrackbarPos("Faza gry ","Ustawienia",0);
	}

	for(int i=0;i<cards.size();i++)
	{
		if(cards[i].dead==true ) {game.server.Dead(cards[i].id); cards.erase(cards.begin() +i); i=-1; }
	}

	//wyswietlenie wszytkich kart
	for(unsigned int i=0;i<cards.size();i++)
	{
		cards[i].Draw(frame,bcards,game);
	}

	for(unsigned int i=0;i<stack.size();i++)
	{
		stack[i].Draw(frame,bcards,game);
	}

	DrawSafeRegion(frame);

	char cad1[100];
	char cad2[100];
	char cad3[100];
	char cad4[100];

	sprintf(cad1,"Kart na polu bitwy: %d",cards.size());
	sprintf(cad2,"Kart na stackie: %d",stack.size());
	sprintf(cad3,"Aktualny gracz: %s",game.getCurrentPlayer().name.c_str());
	sprintf(cad4,"Aktualna faza: %s",game.getCurrentPhase().c_str());


	putText(frame,cad1, Point(10,25),FONT_HERSHEY_SIMPLEX, 0.5,  Scalar(0,0,255),2);
	putText(frame,cad2, Point(10,40),FONT_HERSHEY_SIMPLEX, 0.5,  Scalar(0,0,255),2);
	putText(frame,cad3, Point(10,55),FONT_HERSHEY_SIMPLEX, 0.5,  Scalar(0,0,255),2);
	putText(frame,cad4, Point(10,70),FONT_HERSHEY_SIMPLEX, 0.5,  Scalar(0,0,255),2);

	if(game.t==false)
	{
		stack.clear();
		cards.clear();
	}
}


void FirstCalibration(vector<Marker> markers,Mat &img,Game &game)
{
	int t=0;
	Mat dst;
	dst.cols=game.GetGameWidth();
	dst.rows=game.GetGameHeight();

	if(game.t==false)
	{
		Point a,b,c,d;
		float constans = 212.13f; //Magiczna liczba oznaczająca długość przekątnej markera o rozmiarach 150 x 150
		float distance,rel,xx,yy,xxx,yyy;
		Point2f movement;
		for(int i=0;i<markers.size();i++)
		{

			distance = Card::Distance(markers[i][0],markers[i][2]);
			rel = distance/constans;
			xx = markers[i][0].x - markers[i][2].x;
			yy = markers[i][0].y - markers[i][2].y;
			xxx = xx/(float)150;
			yyy = yy/(float)150;
			movement = Point(xxx*25,yyy*25);
			//circle(img,markers[i][0],4,Scalar(0,0,200),2);
			//markers[i].draw(img,Scalar(200,0,0),3);
			if(markers[i].id==341) 
			{
				t++; 
				game.a=markers[i][0] + movement;
				//circle(img,game.a,4,Scalar(0,0,200),3);
			}
			else if(markers[i].id==1005) 
			{
				t++; 
				game.b=markers[i][0] +movement;
				//circle(img,game.b,4,Scalar(0,0,200),3);
			}
			else if(markers[i].id==791) {
				t++;
				game.c=markers[i][0]+movement;
				//circle(img,game.c,4,Scalar(0,0,200),3);
			}
			else if(markers[i].id==977) {
				t++; 
				game.d=markers[i][0]+movement;
				//circle(img,game.d,4,Scalar(0,0,200),3);
			}
		}
		if(t==4) game.t=true;
	}
	else if(game.t==true)
	{
		Point2f c1[4] = {game.a,game.b,game.c,game.d};
		Point2f c2[4] = {Point2f(0,0), Point2f(game.GetGameWidth(),0), Point2f(game.GetGameWidth(),game.GetGameHeight()),Point2f(0,game.GetGameHeight())};
		Mat mmat(3,3,CV_32FC1);
		mmat=getPerspectiveTransform(c1,c2);
		warpPerspective(img,dst,mmat,Size(game.GetGameWidth(),game.GetGameHeight()));
	}
	imshow("Frame",img);
	img=dst;
}

int Card::ID=0;

int main( int argc, char** argv )
{
	bool presed = false;

	Game game("lukasz",977,"daniel",341,"192.168.0.103",600,1366,768,8,true);
	//	Game game("lukasz",177,"daniel",341,"25.83.48.69",6121,800,600,8,false);
	ScriptsManager scriptManager;
	vector<CardB> dataBaseCards;
	vector<Card> cardsInPlay;
	vector<Card> cardsOnStack;



	Mat frame;	
	VideoCapture capture(0);

	LoadCardDatabase(dataBaseCards);
	for(int i=0;i<dataBaseCards.size();i++)
	{
	//	dataBaseCards[i].PrintStats();
	}
	game.server.AddPlayer(1,"lukasz");
	game.server.AddPlayer(2,"daniel");
	namedWindow("Settings",CV_WINDOW_NORMAL);
	createTrackbar("Threeshold","Settings",&three,300);
	createTrackbar("Balans bieli ","Settings",&white,10000);
	createTrackbar("Min area ","Settings",&minArea,100);
	createTrackbar("Max arrea ","Settings",&maxArea,500);
	createTrackbar("Gracz ","Settings",&gracz,1);
	createTrackbar("Faza gry ","Settings",&faza,4);
	createTrackbar("Metoda ","Settings",&borderMethod,1);
	createTrackbar("UpdateTime ","Settings",&updateTime,10);
	capture.set(CV_CAP_PROP_FRAME_WIDTH, 1280 );
	capture.set(CV_CAP_PROP_FRAME_HEIGHT, 720 );
	capture.set(CV_CAP_PROP_FOCUS, 12 );
	capture.read(frame);

	while(1)
	{
		game.Update();
		game.server.SetInterval(updateTime);
		game.setPlayer(gracz);
		game.setFaza(faza);
		capture.read(frame);
		game.CheckMarkers(frame);
		FirstCalibration(game.markers,frame,game);
		if(frame.data)
		{
			MainGameLogic(frame,cardsInPlay,cardsOnStack,dataBaseCards,game);
			scriptManager.Update(frame,game,cardsInPlay,cardsOnStack);
			line(frame,Point(10,10),Point(10,40),Scalar(200,0,0),3);
			imshow("Preview", frame);
			//game.Draw();
		}
	
		if(cv::waitKey(30)==27) break;
		if(cv::waitKey(10)==113) presed = false;
		if(cv::waitKey(10)==97 && presed==false) { game.nextPhase(); presed=true; }
		if(cv::waitKey(10)==99) cardsOnStack.clear();
	}
	capture.release();
	cv::waitKey(0);
	return 0;
}