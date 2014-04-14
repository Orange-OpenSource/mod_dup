//============================================================================
// Name        : lib_compare.cpp
// Author      : Vallee Cedric
// Version     :
// Copyright   : Orange
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <boost/regex.hpp>
#include <boost/assign.hpp>
#include <time.h>
#include "dtl.hpp"
#include "stringCompare.hh"

static const char alphanum[] =
"0123456789"
"!@#$%^&*"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz";

int stringLength = sizeof(alphanum) - 1;

char genRandom()  // Random string generator function.
{
    return alphanum[rand() % stringLength];
}

void testPerf(LibWsDiff::StringCompare myCmp, int run){
	srand(time(0));
	std::string str1(""),str2;
	for(int i=0;i<30000;++i){
		str1+=genRandom();
	}
	const int test[] = {0,1,10,50,100,500,1000,5000,10000,20000,30000};
	std::vector<int> valueToTest(test,test+10);
	std::string diff;
	for(std::vector<int>::iterator it=valueToTest.begin();it!=valueToTest.end();++it){
		str2=str1;
		for(int j=0;j<*it;++j){
			int rd=0;
			do{
				rd=rand() % str1.length();
			}while(str2[rd]!=str1[rd]);
			str2[rd]=str1[rd]+1;
		}
		clock_t start = clock();
		for(int i=0;i<run;++i){
			myCmp.retrieveDiff(str1,str2,diff);
		}
		printf("%d %d %.3f\n",*it,run, (double)(clock()-start)/CLOCKS_PER_SEC);
	}
}

void testPerfSplit(LibWsDiff::StringCompareBody myCmp, int nBalise, int run){
	srand(time(0));
	std::string str1(""),str2;
	for(int i=0;i<300000;++i){
		str1+=genRandom();
	}
	for(int i=0;i<nBalise;++i){
		int rd=rand() % str1.length();
		str1.insert(rd,"</lbl><lbl>");
	}

	const int test[] = {0,1,10,50,100,500,1000,5000,10000,20000,30000};
	std::vector<int> valueToTest(test,test+10);
	std::string diff;
	for(std::vector<int>::iterator it=valueToTest.begin();it!=valueToTest.end();++it){
		str2=str1;
		for(int j=0;j<*it;++j){
			int rd=0;
			do{
				rd=rand() % str1.length();
			}while(str2[rd]!=str1[rd]);
			str2[rd]=str1[rd]+1;
		}
		clock_t start = clock();
		for(int i=0;i<run;++i){
			myCmp.retrieveDiff(str1,str2,diff);
		}
		printf("%d %d %.3f\n",*it,run, (double)(clock()-start)/CLOCKS_PER_SEC);
	}
}

int main() {
	std::string origine("<header>SuperHeader</header>");
	std::string result("<header>SuparerHeader</header>");

	const char* myStopRegex[] = {"duplicate=False"};
	std::vector<std::string> stopRe(myStopRegex,myStopRegex+1);

	const char* myIgnoreRegex[] = {"test"};
	std::vector<std::string> igRe(myIgnoreRegex,myIgnoreRegex+1);

	LibWsDiff::StringCompare myCompare(stopRe,igRe);

	std::string diffReturn;
	myCompare.retrieveDiff(origine,result,diffReturn);
	std::cout << diffReturn << "\n-----------\n" ;
	myCompare.retrieveDiff("<header>SuperHeader</header>","<header>SperHeader</header>test",diffReturn);
	std::cout << diffReturn << "\n-----------\n" ;
	myCompare.retrieveDiff("<header duplicate=False>SuperHeader</header>","<header>SuperHeader</header>test",diffReturn);
	std::cout << diffReturn << "\n-----------\n" ;

	std::string header1 = "HTTP/1.1 200 OK\nDate: Fri, 31 Jan 2014 11:50:18 GMT\nServer: Apache/x.x.xx (Unix) mod_ssl/x.x.xx OpenSSL/x.x.xk DAV/2 Axis2C/1.6.0\nUNIQUE_ID: CPP55741147\nContent-Length: 1154\nKeep-Alive: timeout=5, max=77\nConnection: Keep-Alive\nContent-Type: text/xml; charset=UTF-8";
	std::string header2 = "HTTP/1.1 200 OK\nDate: Fri, 31 Jan 2014 11:50:17 GMT\nServer: Apache/x.x.xx (Unix) mod_ssl/x.x.xx OpenSSL/x.x.xk DAV/2 Axis2C/1.6.0\nUNIQUE_ID: CPP63076330\nContent-Length: 2717\nKeep-Alive: timeout=5, max=78\nConnection: Keep-Alive\nContent-Type: text/xml; charset=UTF-8";

	std::string body1="<?xml version='1.0' encoding='UTF-8'?>\n<getPNSReturn>\n<Process><Status>OK</Status>\n</Process>\n<Data><XxxxxxxxxxxXxxx><Extension><XXXXXXXXXXXXXXXXXXXXXX><![CDATA[<?xml version='1.0' encoding='UTF-8'?>\n<result>\n\n<unseenmsg>XX</unseenmsg>\n<nbmsg>XX</nbmsg>\n<nextUID>XXXX</nextUID>\n<msgs>\n\n<msg>\n<msg_id>XX</msg_id>\n<msg_from>0XXXXXXXXX &lt;noreplyXXXXXXXX.fr&gt;</msg_from>\n<msg_date>Sun, 7 Mar 2006 15:35:06 +0100 (CET)</msg_date>\n<msg_subject>Demande d'informations</msg_subject>\n<msg_begin_body>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</msg_begin_body>\n<msg_url_access>http://XXXXXXX.XXXXXXX.XX/webmail/fr_FR/read_syn.html?idm=XX%26idb=xxx</msg_url_access>\n</msg>\n\n<msg>\n<msg_id>XX</msg_id>\n<msg_from>xxxxxxx@wanadoo.fr</msg_from>\n<msg_date>Sun, 7 Mar 2006 16:12:52 +0100 (CET)</msg_date>\n<msg_subject>xxxxxxx Msg xxxxxx</msg_subject>\n<msg_begin_body>xxxxxxx Msg2 xxxxxx</msg_begin_body>\n<msg_url_access>http://XXXXXXX.XXXXXXX.XX/webmail/fr_FR/read_syn.html?idm=XX%26idb=xxx</msg_url_access>\n</msg>\n\n</msgs>\n</result>\n]]></AllEmailsLastNDetailed></Extension></SyndicationData></Data></getPNSReturn>";
	std::string body2="<?xml version='1.0' encoding='UTF-8'?>\n<getPNSReturn>\n<Process><Status>OK</Status>\n</Process>\n<Data><PreferencesData><Extension><isHPPDefault>0</isHPPDefault><HPProPref>news.orange.fr</HPProPref><Profil><v1>x</v1><v2>x</v2><v5>x</v5></Profil><MeteoCities>XXXXXXX:Saint-germain-en-genest-sur-papouasie</MeteoCities><DefaultPortal>0</DefaultPortal><OOPSApplications>eJxzj9J2MjMBAAWEAXk=</OOPSApplications></Extension></PreferencesData><SyndicationData><NoMMS>x</NoMMS><NoSMS>x</NoSMS><NoUnReadMails>-1</NoUnReadMails><EmailsLastNDetailed><![CDATA[<?xml version='1.0' encoding='UTF-8'?>\n<result>\n\n<unseenmsg>xx</unseenmsg>\n<nbmsg>xx</nbmsg>\n<nextUID>XXXX</nextUID>\n<msgs>\n\n<msg>\n<msg_id>XX</msg_id>\n<msg_from>0XXXXXXXX &lt;noreply@XXXX.fr&gt;</msg_from>\n<msg_date>Sun, 7 Mar 2006 15:35:06 +0100 (CET)</msg_date>\n<msg_subject>Demande d'informations</msg_subject>\n<msg_begin_body>xxxx super msg xxxxx</msg_begin_body>\n<msg_url_access>http://XXXXXXX.XXXXXXX.XX/webmail/fr_FR/read_syn.html?idm=XX%26idb=xxx</msg_url_access>\n</msg>\n\n<msg>\n<msg_id>xx</msg_id>\n<msg_from>xxxxxxx@wanadoo.fr</msg_from>\n<msg_date>Sun, 7 Mar 2006 16:12:52 +0100 (CET)</msg_date>\n<msg_subject>xxxxxxx Msg xxxxxx</msg_subject>\n<msg_begin_body>xxxxxxx Msg2 xxxxxx</msg_begin_body>\n<msg_url_access>http://XXXXXXX.XXXXXXX.XX/webmail/fr_FR/read_syn.html?idm=XX%26idb=xxx</msg_url_access>\n</msg>\n\n</msgs>\n</result>\n]]></EmailsLastNDetailed><Extension><NoEMails>-1</NoEMails><BmkPortailFibre>DHKJDHKJDHJKSHDJKSDHJKL</BmkPortailFibre><FssPartnersData><item><key>sugarsync</key><value><infos><info><infoKey>UsedStorage</infoKey><infoValue>x</infoValue></info><info><infoKey>MaxStorage</infoKey><infoValue>x</infoValue></info></infos></value></item></FssPartnersData></Extension></SyndicationData><NextECareData><InternetBillList>xxx<Date>19700101</Date></InternetBillList></NextECareData></Data></getPNSReturn>";

	stopRe.clear();
	igRe.clear();
	igRe.push_back("Date: (.*?\\n)");
	igRe.push_back("UNIQUE_ID: (.*?\\n)");
	igRe.push_back("Keep-Alive: (.*?\\n)");
	std::string diff;
	LibWsDiff::StringCompareHeader myCmpHeader(stopRe,igRe);
	std::cout << "\n-----------\n" ;
	myCmpHeader.retrieveDiff(header1,header2,diff);
	std::cout << diff;

	clock_t start = clock();
	for (int i =0;i<10000;++i){
		myCmpHeader.retrieveDiff(header1,header2,diff);
	}
	printf("10000 header comparaison in %.5fs\n", (double)(clock()-start)/CLOCKS_PER_SEC);

	igRe.clear();
	igRe.push_back("<Profil>.*?</Profil>");
	LibWsDiff::StringCompareBody myCmpBody(stopRe,igRe);
	std::cout << "\n-----------\n" ;
	myCmpBody.retrieveDiff(body1,body2,diff);
	std::cout << diff;

	start = clock();
	for (int i =0;i<10000;++i){
		myCmpBody.retrieveDiff(body1,body2,diff);
	}
	printf("10000 body comparaison in %.5fs\n", (double)(clock()-start)/CLOCKS_PER_SEC);

	return 0;
}
