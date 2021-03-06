// ----------------------------------------------------------------------
// File: ShellExecutor.cc
// Author: Michal Kamin Simon - CERN
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
#include "common/Namespace.hh"
#include "common/ShellCmd.hh"
/*----------------------------------------------------------------------------*/
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <uuid/uuid.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
/*----------------------------------------------------------------------------*/
#include "XrdSys/XrdSysTimer.hh"
/*----------------------------------------------------------------------------*/

EOSCOMMONNAMESPACE_BEGIN

/*----------------------------------------------------------------------------*/
ShellCmd::ShellCmd (std::string const & cmd) : cmd (cmd), monitor_active(false), monitor_joined(false)
{
  //----------------------------------------------------------------------------
  // generate the 'uuid' for the 'fifos'
  uuid_t uu;
  uuid_generate(uu);
  uuid_unparse(uu, uuid);

  // create a 'fifo' for 'stdout'
  stdout_name = ShellExecutor::fifo_name(uuid, ShellExecutor::stdout);
  mkfifo(stdout_name.c_str(), 0666);

  // create a 'fifo' for 'stderr'
  stderr_name = ShellExecutor::fifo_name(uuid, ShellExecutor::stderr);
  mkfifo(stderr_name.c_str(), 0666);

  // create a 'fifo' for 'stdin'
  stdin_name = ShellExecutor::fifo_name(uuid, ShellExecutor::stdin);
  mkfifo(stdin_name.c_str(), 0666);

  // execute the command
  pid = ShellExecutor::instance().execute(cmd, uuid);

  // start the monitor thread
  pthread_create(&monitor_thread, 0, run_monitor, this);

  //----------------------------------------------------------------------------
  // open the 'fifos'
  // (the order is not random: it has to match the order in
  // 'ShellExecutor' otherwise the two process will deadlock)
  //----------------------------------------------------------------------------
  outfd = open(stdout_name.c_str(), O_RDONLY);
  infd = open(stdin_name.c_str(), O_WRONLY);
  errfd = open(stderr_name.c_str(), O_RDONLY);
}

/*----------------------------------------------------------------------------*/
ShellCmd::~ShellCmd ()
{
  //----------------------------------------------------------------------------
  // close file descriptors
  //----------------------------------------------------------------------------
  close(outfd);
  close(errfd);
  close(infd);

  //----------------------------------------------------------------------------
  // delete 'fifos'
  //----------------------------------------------------------------------------
  remove(stdout_name.c_str());
  remove(stderr_name.c_str());
  remove(stdin_name.c_str());

  //----------------------------------------------------------------------------
  // kill the 'cmd' if active
  //----------------------------------------------------------------------------
  if (is_active()) kill();

  //----------------------------------------------------------------------------
  // wait for the monitor thread to exit gracefully  
  // (make sure the thread is joined to avoid memory leaks)
  //----------------------------------------------------------------------------
  if (monitor_active || !monitor_joined) pthread_join(monitor_thread, 0);
}

/*----------------------------------------------------------------------------*/
void
ShellCmd::monitor ()
{
  // set the active flag
  monitor_active = true;

  // switch this thread to root to be able to attach

  syscall(SYS_setresuid,0,0,0);
  //----------------------------------------------------------------------------
  // trace the 'command' process (without stopping it),
  // this way the given process becomes its parent
  // and can use 'waitpid' for waiting
  //----------------------------------------------------------------------------
  if ( (ptrace(PTRACE_ATTACH, pid, 0, 0)) == -1)
  {
    // ptrace attach failed, we cannot proceed, but we block until the child terminated
    perror("error: failed to attach to forked process");
    while (is_active())
    {
      XrdSysTimer snooze;
      snooze.Wait(250);
    }
    cmd_stat.exited = false;
    cmd_stat.exit_code = EPERM;
    cmd_stat.signaled = false;
    cmd_stat.signo = 0;
    cmd_stat.status = 0;
    
    // reset the active flag
    monitor_active = false;
    return;
  }

  // wait for the 'command' process
  int status = 0;

  // wait for the process to terminate
  while (true)
  {
    // wait for a change in the process status
    if (waitpid(pid, &status, 0) == pid)
    {
      // if the process has been stopped (not terminated)
      // resume it and keep waiting
      if (status && WIFSTOPPED(status))
      {
        ptrace(PTRACE_CONT, pid, 0, 0);
        continue;
      }
      // if the process has been just resumed keep waiting
      if (status && WIFCONTINUED(status)) continue;
      // otherwise the process is terminated and we are done with waiting
      break;
    }
    else
    {
      perror("error: failed to waitpid for attached process");
      if (!is_active()) break;
      XrdSysTimer snooze;
      // prevent tight loops
      snooze.Wait(250);
    }
  }

  // the status of the 'command' process
  cmd_stat.exited = WIFEXITED(status);
  cmd_stat.exit_code = WEXITSTATUS(status);
  cmd_stat.signaled = WIFSIGNALED(status);
  cmd_stat.signo = WTERMSIG(status);
  cmd_stat.status = status;
  // reset the active flag
  monitor_active = false;
}

/*----------------------------------------------------------------------------*/
void*
ShellCmd::run_monitor (void * ptr)
{
  ShellCmd * const me = static_cast<ShellCmd * const> (ptr);
  me->monitor();
  return 0;
}

/*----------------------------------------------------------------------------*/
cmd_status
ShellCmd::wait ()
{
  if (monitor_active)
  {
    monitor_joined = true;
    pthread_join(monitor_thread, 0);
  }
  return cmd_stat;
}

/*----------------------------------------------------------------------------*/
cmd_status
ShellCmd::wait (size_t timeout)
{
  size_t exp_sleep=1; 
  for (size_t i=0; i< timeout+9; ++i) 
  {
    if (!is_active())
      break;
    XrdSysTimer sleeper;
    sleeper.Wait(exp_sleep);
    if (exp_sleep<512)
      exp_sleep*=2;
    else
      exp_sleep=1000;
  }

  // stop it if the timeout is exceeded
  if (is_active())
    kill();

  if (monitor_active)
  {
    monitor_joined = true;
    pthread_join(monitor_thread, 0);
  }
  return cmd_stat;
}

/*----------------------------------------------------------------------------*/
void
ShellCmd::kill (int sig) const
{
  ::kill(pid, sig);
}

/*----------------------------------------------------------------------------*/
bool
ShellCmd::is_active () const
{
  //----------------------------------------------------------------------------
  // send the null signal to check if the process exists
  // if not 'errno' will be set to 'ESRCH'
  //----------------------------------------------------------------------------
  if (::kill(pid, 0) == -1)
  {
    return errno != ESRCH;
  }
  return true;
}

EOSCOMMONNAMESPACE_END
