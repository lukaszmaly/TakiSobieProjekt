#include "CardB.h"


CardB::CardB(void)
{
}


CardB::~CardB(void)
{
}

CardB::CardB(Mat &img,int id,string name,Color color,Type t)
{
	this->type=t;
	this->color = color;
	this->id = id;
	this->name = name;
	this->img = img;
	if(!img.data)
	{
		cout<<"Blad podczas �adowania pliku: "<<endl;
	}
}
void CardB::Update()
{

	Mat img = imread(name);
	if(!img.data)
	{
		cout<<"Blad podczas �adowania pliku: "<<name<<endl;
	}
}

