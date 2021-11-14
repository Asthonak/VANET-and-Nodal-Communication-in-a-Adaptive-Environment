#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

//char searchstring[100] = "..\\..\\examples_all\\*.cc";
char searchstring[100] = "..\\..\\examples_all\\*.exe";
//char delimiter[20] = " ";
char delimiter[20] = "\n";
int main ()
{
  vector <string> FileNames;
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind = FindFirstFile(searchstring,&FindFileData);
  FileNames.push_back(FindFileData.cFileName);
  std::cout<<FindFileData.cFileName<<endl;
  while (FindNextFile(hFind,&FindFileData))
  {
    std::cout<<FindFileData.cFileName<<endl;
	FileNames.push_back(FindFileData.cFileName);
  }
  for (int i = 0 ; i<FileNames.size(); i++)
  {
   cout<<FileNames[i]<<delimiter;
  
  }

  return 0;
}