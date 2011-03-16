/*----------------------------------------------------------------------------*/
#include "common/Logging.hh"
#include "common/LayoutId.hh"
#include "common/Mapping.hh"
#include "mgm/Policy.hh"
#include "mgm/XrdMgmOfs.hh"
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
void
Policy::GetLayoutAndSpace(const char* path, eos::ContainerMD::XAttrMap &attrmap, const eos::common::Mapping::VirtualIdentity &vid, unsigned long &layoutId, XrdOucString &space, XrdOucEnv &env, unsigned long &forcedfsid) 

{
  // this is for the moment only defaulting or manual selection
  unsigned long layout      = eos::common::LayoutId::GetLayoutFromEnv(env);
  unsigned long xsum        = eos::common::LayoutId::GetChecksumFromEnv(env);
  unsigned long stripes     = eos::common::LayoutId::GetStripeNumberFromEnv(env);
  unsigned long stripewidth = eos::common::LayoutId::GetStripeWidthFromEnv(env);

  
  const char* val=0;
  if ( (val=env.Get("eos.space"))) {
    space = val;
  } else {
    space = "default";
  }

  if (attrmap.count("sys.forced.space")) {
    // we force to use a certain space in this directory even if the user wants something else
    space = attrmap["sys.forced.space"].c_str();
    eos_static_debug("sys.forced.space in %s",path);
  }

  if (attrmap.count("sys.forced.layout")) {
    XrdOucString layoutstring = "eos.layout.type="; layoutstring += attrmap["sys.forced.layout"].c_str();
    XrdOucEnv layoutenv(layoutstring.c_str());
    // we force to use a specified layout in this directory even if the user wants something else
    layout = eos::common::LayoutId::GetLayoutFromEnv(layoutenv);
    eos_static_debug("sys.forced.layout in %s",path);
  }

  if (attrmap.count("sys.forced.checksum")) {
    XrdOucString layoutstring = "eos.layout.checksum="; layoutstring += attrmap["sys.forced.checksum"].c_str();
    XrdOucEnv layoutenv(layoutstring.c_str());
    // we force to use a specified checksumming in this directory even if the user wants something else
    xsum = eos::common::LayoutId::GetChecksumFromEnv(layoutenv);
    eos_static_debug("sys.forced.checksum in %s",path);
  }
  if (attrmap.count("sys.forced.nstripes")) {
    XrdOucString layoutstring = "eos.layout.nstripes="; layoutstring += attrmap["sys.forced.nstripes"].c_str();
    XrdOucEnv layoutenv(layoutstring.c_str());
    // we force to use a specified stripe number in this directory even if the user wants something else
    stripes = eos::common::LayoutId::GetStripeNumberFromEnv(layoutenv);
    eos_static_debug("sys.forced.nstripes in %s",path);
  }

  if (attrmap.count("sys.forced.stripewidth")) {
    XrdOucString layoutstring = "eos.layout.stripewidth="; layoutstring += attrmap["sys.forced.stripewidth"].c_str();
    XrdOucEnv layoutenv(layoutstring.c_str());
    // we force to use a specified stripe width in this directory even if the user wants something else
    stripewidth = eos::common::LayoutId::GetStripeWidthFromEnv(layoutenv);
    eos_static_debug("sys.forced.stripewidth in %s",path);
  }

  if ( ((!attrmap.count("sys.forced.nouserlayout")) || (attrmap["sys.forced.nouserlayout"] != "1")) &&
       ((!attrmap.count("user.forced.nouserlayout")) || (attrmap["user.forced.nouserlayout"] != "1")) ) {

    if (attrmap.count("user.forced.space")) {
      // we force to use a certain space in this directory even if the user wants something else
      space = attrmap["user.forced.space"].c_str();
      eos_static_debug("user.forced.space in %s",path);
    }

    if (attrmap.count("user.forced.layout")) {
      XrdOucString layoutstring = "eos.layout.type="; layoutstring += attrmap["user.forced.layout"].c_str();
      XrdOucEnv layoutenv(layoutstring.c_str());
      // we force to use a specified layout in this directory even if the user wants something else
      layout = eos::common::LayoutId::GetLayoutFromEnv(layoutenv);
      eos_static_debug("user.forced.layout in %s",path);
    }
    
    if (attrmap.count("user.forced.checksum")) {
      XrdOucString layoutstring = "eos.layout.checksum="; layoutstring += attrmap["user.forced.checksum"].c_str();
      XrdOucEnv layoutenv(layoutstring.c_str());
      // we force to use a specified checksumming in this directory even if the user wants something else
      xsum = eos::common::LayoutId::GetChecksumFromEnv(layoutenv);
      eos_static_debug("user.forced.checksum in %s",path);
    }
    if (attrmap.count("user.forced.nstripes")) {
      XrdOucString layoutstring = "eos.layout.nstripes="; layoutstring += attrmap["user.forced.nstripes"].c_str();
      XrdOucEnv layoutenv(layoutstring.c_str());
      // we force to use a specified stripe number in this directory even if the user wants something else
      stripes = eos::common::LayoutId::GetStripeNumberFromEnv(layoutenv);
      eos_static_debug("user.forced.nstripes in %s",path);
    }
    
    if (attrmap.count("user.forced.stripewidth")) {
      XrdOucString layoutstring = "eos.layout.stripewidth="; layoutstring += attrmap["user.forced.stripewidth"].c_str();
      XrdOucEnv layoutenv(layoutstring.c_str());
      // we force to use a specified stripe width in this directory even if the user wants something else
      stripewidth = eos::common::LayoutId::GetStripeWidthFromEnv(layoutenv);
      eos_static_debug("user.forced.stripewidth in %s",path);
    }
  }

  if ( (attrmap.count("sys.forced.nofsselection") && (attrmap["sys.forced.nofsselection"]=="1")) || 
       (attrmap.count("user.forced.nofsselection") && (attrmap["user.forced.nofsselection"]=="1")) ) {
    eos_static_debug("<sys|user>.forced.nofsselection in %s",path);
    forcedfsid = 0;
  } else {
    if ((val = env.Get("eos.force.fsid"))) {
      forcedfsid = strtol(val,0,10);
    } else {
      forcedfsid = 0;
    }
  }
  layoutId = eos::common::LayoutId::GetId(layout, xsum, stripes, stripewidth);
  return; 
}


/*----------------------------------------------------------------------------*/
bool 
Policy::Set(const char* value) 
{
  XrdOucEnv env(value);
  XrdOucString policy=env.Get("mgm.policy");

  XrdOucString skey=env.Get("mgm.policy.key");
  
  XrdOucString policycmd = env.Get("mgm.policy.cmd");

  if (!skey.length())
    return false;

  bool set=false;

  if (!value) 
    return false;

  //  gOFS->ConfigEngine->SetConfigValue("policy",skey.c_str(), svalue.c_str());
  
  return set;
}

/*----------------------------------------------------------------------------*/
bool
Policy::Set(XrdOucEnv &env, int &retc, XrdOucString &stdOut, XrdOucString &stdErr)
{
  int envlen;
  // no '&' are allowed into stdOut !
  XrdOucString inenv = env.Env(envlen);
  while(inenv.replace("&"," ")) {};
  bool rc = Set(env.Env(envlen));
  if (rc == true) {
    stdOut += "success: set policy [ "; stdOut += inenv; stdOut += "]\n";
    errno = 0;
    retc = 0;
    return true;
  } else {
    stdErr += "error: failed to set policy [ "; stdErr += inenv ; stdErr += "]\n";
    errno = EINVAL;
    retc = EINVAL;
    return false;
  }
}

/*----------------------------------------------------------------------------*/
void 
Policy::Ls(XrdOucEnv &env, int &retc, XrdOucString &stdOut, XrdOucString &stdErr)
{
  
}

/*----------------------------------------------------------------------------*/
bool
Policy::Rm(XrdOucEnv &env, int &retc, XrdOucString &stdOut, XrdOucString &stdErr)
{
  return true;
}

/*----------------------------------------------------------------------------*/
const char* 
Policy::Get(const char* key) {
  return 0;
}
