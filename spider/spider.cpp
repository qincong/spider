

// spider.cpp : 定义控制台应用程序的入口点。
//




#include "stdafx.h"

//#include <Windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "winsock2.h"
#include <time.h>
#include <queue>
#include <hash_set>
#include <unordered_set>

#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define DEFAULT_PAGE_BUF_SIZE 1048576

queue<string> hrefUrl;
hash_set<string> visitedUrl;
hash_set<string> visitedImg;
int depth = 0;
int g_ImgCnt = 1;

//解析主机名，资源名
bool ParseURL(const string &url, string &host, string &resource) {
	if (strlen(url.c_str()) > 2000) {
		return false;
	}
	const char * pos = strstr(url.c_str(), "http://");
	if (pos == NULL)
		pos = url.c_str();
	else pos += strlen("http://");	//把pos移到http后面
	if (strstr(pos, "/") == 0)
		return false;
	char pHost[100];
	char pResource[2000];
	sscanf(pos, "%[^/]%s", pHost, pResource);
	host = pHost;
	resource = pResource;
	return true;
}

bool GetHttpResponse(const string &url, char * &response, int &bytesRead) {
	string host, resource;
	if (!ParseURL(url, host, resource)) {
		cout << "Can not parse the url" << endl;
		return false;
	}

	//建立socket
	struct hostent * hp = gethostbyname(host.c_str());
	if (hp == NULL) {
		cout << "Can not find host address" << endl;
		return false;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 || sock == -2) {
		cout << "Can not creat sock." << endl;
		return false;
	}

	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(80);
	memcpy(&sa.sin_addr, hp->h_addr, 4);
	if (0 != connect(sock, (SOCKADDR*)&sa, sizeof(sa))) {
		cout << "Can not connect: " << url << endl;
		closesocket(sock);
		return false;
	}
	string request = "GET " + resource + "HTTP/1.1\r\nHost:" + host + "\r\nConnection:Close\r\n\r\n";
	if (SOCKET_ERROR == send(sock, request.c_str(), request.size(), 0)) {
		cout << "send error" << endl;
		closesocket(sock);
		return false;
	}

	//接收数据
	int m_nContentLength = DEFAULT_PAGE_BUF_SIZE;
	char *pageBuf = (char *)malloc(m_nContentLength);
	memset(pageBuf, 0, m_nContentLength);
	bytesRead = 0;
	int ret = 1;
	cout << "Read: ";
	while (ret > 0) {
		ret = recv(sock, pageBuf + bytesRead, m_nContentLength - bytesRead, 0);
		if (ret > 0) {
			bytesRead += ret;
		}
		if (m_nContentLength - bytesRead < 100) {
			cout << "\nRealloc memorry" << endl;
			m_nContentLength *= 2;
			pageBuf = (char *)realloc(pageBuf, m_nContentLength);
		}
		cout << ret << " ";
	}
	cout << endl;
	pageBuf[bytesRead] = '\0';
	response = pageBuf;
	closesocket(sock);
	return true;
}

void HTMLParse(string & htmlResponse, vector<string> & imgurls, const string &host) {
	const char *p = htmlResponse.c_str();
	char *tag = "href=\"";
	const char *pos = strstr(p, tag);
	ofstream ofile("url.txt", ios::app);
	while (pos) {
		pos += strlen(tag);
		const char * nextQ = strstr(pos, "\"");
		if (nextQ) {
			char *url = new char[nextQ - pos + 1];
			sscanf(pos, "%[^\"]", url);
			string surl = url;
			if (visitedUrl.find(surl) == visitedUrl.end()) {
				visitedUrl.insert(surl);
				ofile << surl << endl;
				hrefUrl.push(surl);
			}
			pos = strstr(pos, tag);
			delete[] url;
		}
	}
	ofile << endl << endl;
	ofile.close();

	tag = "<img ";
	const char * att1 = "src=\"";
	const char* att2 = "lazy-src=\"";
	const char *pos0 = strstr(p, tag);
	while (pos0) {
		pos0 += strlen(tag);
		const char *pos2 = strstr(pos0, att2);
		if (!pos2 || pos2 > strstr(pos0, ">")) {
			pos = strstr(pos0,att1);
			if (!pos) {
				pos0 = strstr(att1, tag);
				continue;
			}
			else {
				pos = pos + strlen(att1);
			}
		}
		else {
			pos = pos2 + strlen(att2);
		}	//pos是地址开始的地方
		const char * nextQ = strstr(pos, "\"");
		if (nextQ) {
			char *url = new char[nextQ - pos + 1];
			sscanf(pos, "%[^\"]", url);
			cout << url << endl;
			string imgUrl = url;
			if (visitedImg.find(imgUrl) == visitedImg.end()) {
				visitedImg.insert(imgUrl);
				imgurls.push_back(imgUrl);
			}
			pos0 = strstr(pos0, tag);
			delete[] url;
		}
	}
	cout << "end of Parse this html" << endl;
}

string ToFileName(const string &url) {
	string fileName;
	fileName.resize(url.size());
	int k = 0;
	for (int i = 0; i<(int)url.size(); i++) {
		char ch = url[i];
		if (ch != '\\'&&ch != '/'&&ch != ':'&&ch != '*'&&ch != '?'&&ch != '"'&&ch != '<'&&ch != '>'&&ch != '|')
			fileName[k++] = ch;
	}
	return fileName.substr(0, k) + ".txt";
}

void DownLoadImg(vector<string> &imgurls, const string &url) {
	string foldname = ToFileName(url);
	foldname = "./img/" + foldname;
	if (!CreateDirectory(foldname.c_str(), NULL))
		cout << "Can not create directory:" << foldname << endl;
	char *image;
	int byteRead;
	for (int i = 0; i < imgurls.size(); i++) {
		string str = imgurls[i];
		int pos = str.find_last_of(".");
		if (pos == string::npos)
			continue;
		else {
			string ext = str.substr(pos + 1, str.size() - pos - 1);
			if (ext != "bmp"&&ext != "jpg"&&ext != "jpeg"&&ext != "gif"&&ext != "png")
				continue;
		}
		if (GetHttpResponse(imgurls[i], image, byteRead)) {
			if (strlen(image) == 0) {
				continue;
			}
			const char *p = image;
			const char *pos = strstr(p, "\r\n\r\n") + strlen("\r\n\r\n");
			int index = imgurls[i].find_last_of("/");
			if (index != string::npos) {
				string imgname = imgurls[i].substr(index, imgurls[i].size());
				ofstream ofile(foldname + imgname, ios::binary);
				if (!ofile.is_open())
					continue;
				cout << g_ImgCnt++ << foldname + imgname << endl;
				ofile.write(pos, byteRead - (pos - p));
				ofile.close();
			}
		}
	}
}

void BFS(const string & url) {
	char * response;
	int bytes;
	if (!GetHttpResponse(url, response, bytes)) {
		cout << "The url is wrong!ignore." << endl;
		return;
	}
	string httpResponse = response;
	free(response);
	string filename = ToFileName(url);
	ofstream ofile("./html/" + filename);
	if (ofile.is_open()) {
		ofile << httpResponse << endl;
		ofile.close();
	}
	vector<string> imgurls;
	HTMLParse(httpResponse, imgurls, url);
	DownLoadImg(imgurls, url);
}

void main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return;
	}
	CreateDirectory("./img", 0);
	CreateDirectory("./html", 0);
	string urlStart = "http://tieba.baidu.com/f/index/forumpark?cn=%BA%AB%B9%FA%C3%F7%D0%C7&ci=0&pcn=%D3%E9%C0%D6%C3%F7%D0%C7&pci=0&ct=1&rn=20&pn=1";
	BFS(urlStart);
	visitedUrl.insert(urlStart);
	while (hrefUrl.size() != 0) {
		string url = hrefUrl.front();
		cout << url << endl;
		BFS(url);
		hrefUrl.pop();
	}
	WSACleanup();
	return;
}