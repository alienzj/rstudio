/*
 * RActiveSessions.cpp
 *
 * Copyright (C) 2009-19 by RStudio, PBC
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include <core/r_util/RActiveSessions.hpp>

#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <core/StringUtils.hpp>
#include <core/FileSerializer.hpp>

#include <core/system/System.hpp>
#include <core/system/FileMonitor.hpp>

#include <core/r_util/RSessionContext.hpp>

#define kSessionDirPrefix "session-"

namespace rstudio {
namespace core {
namespace r_util {

namespace {


} // anonymous namespace


void ActiveSession::writeProperty(const std::string& name,
                                 const std::string& value) const
{
   FilePath propertyFile = propertiesPath_.completeChildPath(name);
   Error error = core::writeStringToFile(propertyFile, value);
   if (error)
      LOG_ERROR(error);
}

std::string ActiveSession::readProperty(const std::string& name) const
{
   using namespace rstudio::core;
   FilePath readPath = propertiesPath_.completeChildPath(name);
   if (readPath.exists())
   {
      std::string value;
      Error error = core::readStringFromFile(readPath, &value);
      if (error)
      {
         LOG_ERROR(error);
         return std::string();
      }
      return boost::algorithm::trim_copy(value);
   }
   else
   {
      return std::string();
   }
}

// Start of ActiveSessions =============================================================================================
Error ActiveSessions::create(const std::string& project,
                             const std::string& workingDir,
                             bool initial,
                             std::string* pId) const
{
   // generate a new id (loop until we find a unique one)
   std::string id;
   FilePath dir;
   while (id.empty())
   {
      std::string candidateId = core::r_util::generateScopeId();
      dir = getScratchPath(candidateId);
      if (!dir.exists())
         id = candidateId;
   }

   // create the directory
   Error error = dir.ensureDirectory();
   if (error)
      return error;

   // write initial settings
   ActiveSession activeSession(id, dir);
   activeSession.setProject(project);
   activeSession.setWorkingDir(workingDir);
   activeSession.setInitial(initial);
   activeSession.setLastUsed();
   activeSession.setRunning(false);

   // return the id
   *pId = id;
   return Success();
}

namespace {

bool compareActivityLevel(const ActiveSession& a, const ActiveSession& b)
{
   if (a.executing() == b.executing())
   {
      if (a.running() == b.running())
      {
         if (a.lastUsed() == b.lastUsed())
         {
            return a.id() > b.id();
         }
         else
         {
            return a.lastUsed() > b.lastUsed();
         }
      }
      else
      {
         return a.running();
      }
   }
   else
   {
      return a.executing();
   }
}

} // anonymous namespace

std::vector<boost::shared_ptr<ActiveSession> > ActiveSessions::list(
                                       const FilePath& userHomePath,
                                       bool projectSharingEnabled) const
{
   // We need to create and sort a vector of stack-allocated ActiveSession objects because attempting to do this with
   // any type of pointer leads to occasional segfaults under some circumstances.
   std::vector<ActiveSession> sessions;

   // enumerate children and check for sessions
   std::vector<FilePath> children;
   Error error = storagePath_.getChildren(children);
   if (error)
   {
      LOG_ERROR(error);
      return std::vector<boost::shared_ptr<ActiveSession> >();
   }
   std::string prefix = kSessionDirPrefix;
   for (const FilePath& child : children)
   {
      if (boost::algorithm::starts_with(child.getFilename(), prefix))
      {
         std::string id = child.getFilename().substr(prefix.length());
         FilePath scratchPath = getScratchPath(id);
         ActiveSession session = scratchPath.exists() ? ActiveSession(id, scratchPath) : ActiveSession(id);
         if (!session.empty())
         {
            if (session.validate(userHomePath, projectSharingEnabled))
            {
               sessions.push_back(session);
            }
            else
            {
               // remove sessions that don't have required properties
               // (they may be here as a result of a race condition where
               // they are removed but then suspended session data is
               // written back into them)
               Error error = session.destroy();
               if (error)
                  LOG_ERROR(error);
            }
         }
      }

   }

   // sort by activity level (most active sessions first)
   std::sort(sessions.begin(), sessions.end(), compareActivityLevel);

   // list to return
   std::vector<boost::shared_ptr<ActiveSession> > sharedSessions;
   std::transform(
      sessions.begin(),
      sessions.end(),
      std::back_inserter(sharedSessions),
      [](const ActiveSession& session)
      {
         return boost::shared_ptr<ActiveSession>(new ActiveSession(session));
      });

   // return
   return sharedSessions;
}

size_t ActiveSessions::count(const FilePath& userHomePath,
                             bool projectSharingEnabled) const
{
   return list(userHomePath, projectSharingEnabled).size();
}

boost::shared_ptr<ActiveSession> ActiveSessions::get(const std::string& id) const
{
   FilePath scratchPath = getScratchPath(id);
   if (scratchPath.exists())
      return boost::shared_ptr<ActiveSession>(new ActiveSession(id,
                                                                scratchPath));
   else
      return emptySession(id);
}

boost::shared_ptr<ActiveSession> ActiveSessions::emptySession(
      const std::string& id)
{
   return boost::shared_ptr<ActiveSession>(new ActiveSession(id));
}

FilePath ActiveSessions::getScratchPath(const std::string& id) const
{
   return storagePath_.completeChildPath(kSessionDirPrefix + id);
}

// Start of GlobalActiveSessions =======================================================================================
std::vector<boost::shared_ptr<GlobalActiveSession> >
GlobalActiveSessions::list() const
{
   std::vector<boost::shared_ptr<GlobalActiveSession> > sessions;

   // get all active sessions for the system
   FilePath activeSessionsDir = rootPath_;
   if (!activeSessionsDir.exists())
      return sessions; // no active sessions exist

   std::vector<FilePath> sessionFiles;
   Error error = activeSessionsDir.getChildren(sessionFiles);
   if (error)
   {
      LOG_ERROR(error);
      return sessions;
   }

   for (const FilePath& sessionFile : sessionFiles)
   {
      sessions.push_back(boost::shared_ptr<GlobalActiveSession>(new GlobalActiveSession(sessionFile)));
   }

   return sessions;
}

boost::shared_ptr<GlobalActiveSession>
GlobalActiveSessions::get(const std::string& id) const
{
   FilePath sessionFile = rootPath_.completeChildPath(id);
   if (!sessionFile.exists())
      return boost::shared_ptr<GlobalActiveSession>();

   return boost::shared_ptr<GlobalActiveSession>(new GlobalActiveSession(sessionFile));
}


namespace {

void notifyCountChanged(boost::shared_ptr<ActiveSessions> pSessions,
                        const FilePath& userHomePath,
                        bool projectSharingEnabled,
                        boost::function<void(size_t)> onCountChanged)
{
   onCountChanged(pSessions->count(userHomePath, projectSharingEnabled));
}

} // anonymous namespace

void trackActiveSessionCount(const FilePath& rootStoragePath,
                             const FilePath& userHomePath,
                             bool projectSharingEnabled,
                             boost::function<void(size_t)> onCountChanged)
{

   boost::shared_ptr<ActiveSessions> pSessions(
                                          new ActiveSessions(rootStoragePath));

   core::system::file_monitor::Callbacks cb;
   cb.onRegistered = boost::bind(notifyCountChanged,
                                 pSessions,
                                 userHomePath,
                                 projectSharingEnabled,
                                 onCountChanged);
   cb.onFilesChanged = boost::bind(notifyCountChanged,
                                   pSessions,
                                   userHomePath,
                                   projectSharingEnabled,
                                   onCountChanged);
   cb.onRegistrationError = boost::bind(log::logError, _1, ERROR_LOCATION);

   core::system::file_monitor::registerMonitor(
                   pSessions->storagePath(),
                   false,
                   boost::function<bool(const FileInfo&)>(),
                   cb);
}

} // namespace r_util
} // namespace core
} // namespace rstudio



