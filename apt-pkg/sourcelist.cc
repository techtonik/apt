// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.cc,v 1.3 1998/07/12 23:58:36 jgg Exp $
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/sourcelist.h"
#endif

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <strutl.h>

#include <fstream.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
									/*}}}*/

// SourceList::pkgSourceList - Constructors				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::pkgSourceList()
{
}

pkgSourceList::pkgSourceList(string File)
{
   Read(File);
}
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   return Read(_config->Find("APT::Etc:sourcelist"));
}
									/*}}}*/
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string File)
{
   // Open the stream for reading
   ifstream F(File.c_str(),ios::in | ios::nocreate);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream","Opening %s",File.c_str());
   
   List.erase(List.begin(),List.end());
   char Buffer[300];

   int CurLine = 0;
   while (F.eof() == false)
   {
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      _strtabexpand(Buffer,sizeof(Buffer));
      _strstrip(Buffer);
      
      // Comment or blank
      if (Buffer[0] == '#' || Buffer[0] == 0)
	 continue;
      
      // Grok it
      string Type;
      string URI;
      Item Itm;
      char *C = Buffer;
      if (ParseQuoteWord(C,Type) == false)
	 return _error->Error("Malformed line %u in source list %s (type)",CurLine,File.c_str());
      if (ParseQuoteWord(C,URI) == false)
	 return _error->Error("Malformed line %u in source list %s (URI)",CurLine,File.c_str());
      if (ParseQuoteWord(C,Itm.Dist) == false)
	 return _error->Error("Malformed line %u in source list %s (dist)",CurLine,File.c_str());
      if (Itm.SetType(Type) == false)
	 return _error->Error("Malformed line %u in source list %s (type parse)",CurLine,File.c_str());
      if (Itm.SetURI(URI) == false)
	 return _error->Error("Malformed line %u in source list %s (URI parse)",CurLine,File.c_str());

      // Check for an absolute dists specification.
      if (Itm.Dist.empty() == false && Itm.Dist[Itm.Dist.size() - 1] == '/')
      {
	 if (ParseQuoteWord(C,Itm.Section) == true)
	    return _error->Error("Malformed line %u in source list %s (Absolute dist)",CurLine,File.c_str());
	 Itm.Dist = SubstVar(Itm.Dist,"$(ARCH)",_config->Find("APT::Architecture"));
	 List.push_back(Itm);
	 continue;
      }

      // Grab the rest of the dists
      if (ParseQuoteWord(C,Itm.Section) == false)
	    return _error->Error("Malformed line %u in source list %s (dist parse)",CurLine,File.c_str());
      
      do
      {
	 List.push_back(Itm);
      }
      while (ParseQuoteWord(C,Itm.Section) == true);
   }
   return true;
}
									/*}}}*/
// SourceList::SanitizeURI - Hash the uri				/*{{{*/
// ---------------------------------------------------------------------
/* This converts a URI into a safe filename. It quotes all unsafe characters
   and converts / to _ and removes the scheme identifier. */
string pkgSourceList::SanitizeURI(string URI)
{
   string::const_iterator I = URI.begin() + URI.find(':') + 1;
   for (; I < URI.end() && *I == '/'; I++);

   // "\x00-\x20{}|\\\\^\\[\\]<>\"\x7F-\xFF";
   URI = QuoteString(string(I,URI.end() - I),"\\|{}[]<>\"^~_=!@#$%^&*");
   string::iterator J = URI.begin();
   for (; J != URI.end(); J++)
      if (*J == '/') 
	 *J = '_';
   return URI;
}
									/*}}}*/
// SourceList::MatchPkgFile - Find the package file that has the ver	/*{{{*/
// ---------------------------------------------------------------------
/* This will return List.end() if it could not find the matching 
   file */
pkgSourceList::const_iterator pkgSourceList::MatchPkgFile(pkgCache::VerIterator Ver)
{
   string Base = _config->Find("APT::Architecture");
   for (const_iterator I = List.begin(); I != List.end(); I++)
   {
      string URI = I->PackagesURI();
      switch (I->Type)
      {
	 case Item::Deb:
/*	 if (Base + SanitizeURI(URI) == Ver.File().FileName())
	    return I;*/
	 break;
      };      
   }
   return List.end();
}
									/*}}}*/
// SourceList::Item << - Writes the item to a stream			/*{{{*/
// ---------------------------------------------------------------------
/* This is not suitable for rebuilding the sourcelist file but it good for
   debugging. */
ostream &operator <<(ostream &O,pkgSourceList::Item &Itm)
{
   O << Itm.Type << ' ' << Itm.URI << ' ' << Itm.Dist << ' ' << Itm.Section;
   return O;
}
									/*}}}*/
// SourceList::Item::SetType - Sets the distribution type		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Item::SetType(string S)
{
   if (S == "deb")
   {
      Type = Deb;
      return true;
   }

   return true;
}
									/*}}}*/
// SourceList::Item::SetURI - Set the URI				/*{{{*/
// ---------------------------------------------------------------------
/* For simplicity we strip the scheme off the uri */
bool pkgSourceList::Item::SetURI(string S)
{
   if (S.empty() == true)
      return false;

   if (S.find(':') == string::npos)
      return false;

   S = SubstVar(S,"$(ARCH)",_config->Find("APT::Architecture"));
   
   // Make sure that the URN is / postfixed
   URI = S;
   if (URI[URI.size() - 1] != '/')
      URI += '/';
   
   return true;
}
									/*}}}*/
// SourceList::Item::PackagesURI - Returns a URI to the packages file	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::PackagesURI() const
{
   string Res;
   switch (Type)
   {
      case Deb:
      if (Dist[Dist.size() - 1] == '/')
	 Res = URI + Dist;
      else
	 Res = URI + "dists/" + Dist + '/' + Section +
	 "/binary-" + _config->Find("APT::Architecture") + '/';
      
      Res += "Packages";
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::PackagesInfo - Shorter version of the URI		/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string pkgSourceList::Item::PackagesInfo() const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res += SiteOnly(URI) + ' ';
      if (Dist[Dist.size() - 1] == '/')
	 Res += Dist;
      else
	 Res += Dist + '/' + Section;
      
      Res += " Packages";
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::ArchiveInfo - Shorter version of the archive spec	/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string pkgSourceList::Item::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res += SiteOnly(URI) + ' ';
      if (Dist[Dist.size() - 1] == '/')
	 Res += Dist;
      else
	 Res += Dist + '/' + Section;
      
      Res += " ";
      Res += Ver.ParentPkg().Name();
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::ArchiveURI - Returns a URI to the given archive	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::ArchiveURI(string File) const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res = URI + File;
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::SiteOnly - Strip off the path part of a URI	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::SiteOnly(string URI) const
{
   unsigned int Pos = URI.find(':');
   if (Pos == string::npos || Pos + 3 > URI.length())
      return URI;
   if (URI[Pos + 1] != '/' || URI[Pos + 2] != '/')
      return URI;

   Pos = URI.find('/',Pos + 3);
   if (Pos == string::npos)
      return URI;
   return string(URI,0,Pos);
}
									/*}}}*/
