// ----------------------------------------------------------------------
// File: com_find.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

/*----------------------------------------------------------------------------*/
#include "console/ConsoleMain.hh"
/*----------------------------------------------------------------------------*/

extern int com_file (char*);

/* Find files/directories */
int
com_find (char* arg1) {
  // split subcommands
  XrdOucString oarg=arg1;

  XrdOucTokenizer subtokenizer(arg1);
  subtokenizer.GetLine();
  XrdOucString s1;
  XrdOucString path;
  XrdOucString option="";
  XrdOucString attribute="";
  XrdOucString olderthan="";
  XrdOucString youngerthan="";
  XrdOucString printkey="";
  XrdOucString filter="";
  XrdOucString stripes="";

  XrdOucString in = "mgm.cmd=find&"; 
  while ( (s1 = subtokenizer.GetToken()).length() && (s1.beginswith("-")) ) {
    if (s1 == "-s") {
      option +="s";
    }
    
    if (s1 == "-d") {
      option +="d";
    }
    
    if (s1 == "-f") {
      option +="f";
    }
    
    if (s1 == "-0") {
      option +="f0";
    }

    if (s1 == "-m") {
      option += "fG";
    }

    if (s1 == "--size") {
      option += "S";
    }


    if (s1 == "--fs") {
      option += "L";
    }

    if (s1 == "--checksum") {
      option += "X";
    }


    if (s1 == "--ctime") {
      option += "C";
    }

    if (s1 == "--mtime") {
      option += "M";
    }

    if (s1 == "--fid") {
      option += "F";
    }

    if (s1 == "--nrep") {
      option += "R";
    }

    if (s1 == "--nunlink") {
      option += "U";
    }

    if (s1 == "--stripediff") {
      option += "D";
    }

    if (s1 == "--count") {
      option += "Z";
    }

    if (s1 == "-1") {
      option += "1";
    }

    if (s1.beginswith( "-h" ) || (s1.beginswith( "--help"))) {
      goto com_find_usage;
   }

    if (s1 == "-x") {
      option += "x";

      attribute = subtokenizer.GetToken();

      if (!attribute.length())
        goto com_find_usage;

      if ((attribute.find("&")) != STR_NPOS)
        goto com_find_usage;
    }

    if (s1 == "-ctime") {
      XrdOucString period="";
      period = subtokenizer.GetToken();

      if (!period.length())
	goto com_find_usage;

      bool do_olderthan; 
      do_olderthan = false;
      bool do_youngerthan;
      do_youngerthan = false;

      if (period.beginswith("+")) {
	do_olderthan=true;
      }

      if (period.beginswith("-")) {
	do_youngerthan=true;
      }
      
      if ((!do_olderthan) && (!do_youngerthan)) {
	goto com_find_usage;
      }

      period.erase(0,1);
      time_t now = time(NULL);
      now -= (86400 * strtoul(period.c_str(),0,10));
      char snow[1024];
      snprintf(snow, sizeof(snow)-1, "%lu", now);
      if (do_olderthan) {
	olderthan = snow;
      }
      if (do_youngerthan) {
	youngerthan = snow;
      }
    }

    if (s1 == "-c") {

      option += "c";
      
      filter = subtokenizer.GetToken();
      if (!filter.length()) 
        goto com_find_usage;
      
      if ((filter.find("%%")) != STR_NPOS) {
        goto com_find_usage;
      }
    }

    if (s1 == "-layoutstripes") {
      stripes = subtokenizer.GetToken();
      if (!stripes.length()) 
        goto com_find_usage;
    }

    if (s1 == "-p") {
      option += "p";
      
      printkey = subtokenizer.GetToken();
      
      if (!printkey.length()) 
        goto com_find_usage;
    }

    if (s1 == "-b") {
      option += "b";
    }
  }
  
  if (s1.length()) {
    path = s1;
  }

  if (path == "help") {
      goto com_find_usage;
  }

  // the find to change a layout
  if ( (stripes.length()) ) {
    XrdOucString subfind = oarg;
    XrdOucString repstripes= " "; repstripes += stripes; repstripes += " ";
    subfind.replace("-layoutstripes","");
    subfind.replace(repstripes," -f -s ");
    int rc = com_find((char*)subfind.c_str());
    std::vector<std::string> files_found;
    files_found.clear();
    command_result_stdout_to_vector(files_found);
    std::vector<std::string>::const_iterator it;
    unsigned long long cnt=0;
    unsigned long long goodentries=0;
    unsigned long long badentries=0;
    for (unsigned int i=0; i< files_found.size(); i++) {
      if (!files_found[i].length())
        continue;

      XrdOucString cline="layout "; 
      cline += files_found[i].c_str();
      cline += " -stripes "; 
      cline += stripes;
      rc = com_file((char*)cline.c_str());
      if (rc) {
        badentries++;
      } else {
        goodentries++;
      }
      cnt++;
    }
    rc = 0;
    if (!silent) {
      fprintf(stderr,"nentries=%llu good=%llu bad=%llu\n", cnt, goodentries,badentries);
    }
    return 0;
  }

  // the find with consistency check 
  if ( (option.find("c")) != STR_NPOS ) {
    XrdOucString subfind = oarg;
    subfind.replace("-c","-s -f");
    subfind.replace(filter,"");
    int rc = com_find((char*)subfind.c_str());
    std::vector<std::string> files_found;
    files_found.clear();
    command_result_stdout_to_vector(files_found);
    std::vector<std::string>::const_iterator it;
    unsigned long long cnt=0;
    unsigned long long goodentries=0;
    unsigned long long badentries=0;
    for (unsigned int i=0; i< files_found.size(); i++) {
      if (!files_found[i].length())
        continue;

      XrdOucString cline="check "; 
      cline += files_found[i].c_str();
      cline += " "; 
      cline += filter;
      rc = com_file((char*)cline.c_str());
      if (rc) {
        badentries++;
      } else {
        goodentries++;
      }
      cnt++;
    }
    rc = 0;
    if (!silent) {
      fprintf(stderr,"nentries=%llu good=%llu bad=%llu\n", cnt, goodentries,badentries);
    }
    return 0;
  }


  path = abspath(path.c_str());

  if (!s1.length() && (path=="/")) {
    fprintf(stderr,"error: you didnt' provide any path and would query '/' - will not do that!\n");
    return EINVAL;
  }

  in += "mgm.path=";
  in += path;
  in += "&mgm.option=";
  in += option;
  if (attribute.length()) {
    in += "&mgm.find.attribute=";
    in += attribute;
  }
  if (olderthan.length()) {
    in += "&mgm.find.olderthan=";
    in += olderthan;
  }

  if (youngerthan.length()) {
    in += "&mgm.find.youngerthan=";
    in += youngerthan;
  }

  if (printkey.length()) {
    in += "&mgm.find.printkey=";
    in += printkey;
  }

  XrdOucEnv* result;
  result = client_user_command(in);
  if ( ( option.find("s") ) == STR_NPOS) {
    global_retc = output_result(result);
  } else {
    if (result) {
      global_retc = 0;
    } else {
      global_retc = EINVAL;
    }
  }
  return (0);

 com_find_usage:
  fprintf(stdout,"usage: find [--count] [-s] [-d] [-f] [-0] [-1] [-ctime +<n>|-<n>] [-m] [-x <key>=<val>] [-p <key>] [-b] [-c %%tags] [-layoutstripes <n>] <path>\n");
  fprintf(stdout,"                                                                        -f -d :  find files(-f) or directories (-d) in <path>\n");
  fprintf(stdout,"                                                               -x <key>=<val> :  find entries with <key>=<val>\n");
  fprintf(stdout,"                                                                           -0 :  find 0-size files \n");
  fprintf(stdout,"                                                                           -g :  find files with mixed scheduling groups\n");
  fprintf(stdout,"                                                                     -p <key> :  additionally print the value of <key> for each entry\n");
  fprintf(stdout,"                                                                           -b :  query the server balance of the files found\n");
  fprintf(stdout,"                                                                    -c %%tags  :  find all files with inconsistencies defined by %%tags [ see help of 'file check' command]\n");
  fprintf(stdout,"                                                                           -s :  run as a subcommand (in silent mode)\n");
  fprintf(stdout,"                                                                  -ctime +<n> :  find files older than <n> days\n");
  fprintf(stdout,"                                                                  -ctime -<n> :  find files younger than <n> days\n");
  fprintf(stdout,"                                                           -layoutstripes <n> :  apply new layout with <n> stripes to all files found\n");
  fprintf(stdout,"                                                                           -1 :  find files which are atleast 1 hour old\n");
  fprintf(stdout,"                                                                 --stripediff :  find files which have not the nominal number of stripes(replicas)\n");
  fprintf(stdout,"                                                                      --count :  just print counters for files/dirs found\n");
  fprintf(stdout,"                                                                      default :  find files and directories\n");
  fprintf(stdout,"       find [--nrep] [--nunlink] [--size] [--fid] [--fs] [--checksum] [--ctime] [--mtime] <path>   :  find files and print out the requested meta data as key value pairs\n");              
  return (0);
}
