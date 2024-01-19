#include "oklt/core/attribute_manager/backend_attribute_map.h"
#include "oklt/core/transpiler_session/transpiler_session.h"

namespace oklt {
using namespace clang;

bool BackendAttributeMap::registerHandler(KeyType key,
                     AttrDeclHandler handler)
{
  auto ret = _declHandlers.insert(std::make_pair(std::move(key), std::move(handler)));
  return ret.second;
}

bool BackendAttributeMap::registerHandler(KeyType key,
                     AttrStmtHandler handler)
{
  auto ret = _stmtHandlers.insert(std::make_pair(std::move(key), std::move(handler)));
  return ret.second;
}

bool BackendAttributeMap::handleAttr(const Attr *attr,
                                     const Decl *decl,
                                     SessionStage &session)
{
  std::string name = attr->getNormalizedFullName();
  auto backend = session.getBackend();
  auto it = _declHandlers.find(std::make_tuple(backend, name));
  if(it == _declHandlers.end()) {
    return false;
  }
  return it->second.handle(attr, decl, session);
}

bool BackendAttributeMap::handleAttr(const Attr *attr,
                                     const Stmt *stmt,
                                     SessionStage &session)
{
  std::string name = attr->getNormalizedFullName();
  auto backend = session.getBackend();
  auto it = _stmtHandlers.find(std::make_tuple(backend, name));
  if(it == _stmtHandlers.end()) {
    return false;
  }
  return it->second.handle(attr, stmt, session);
}

bool BackendAttributeMap::hasAttrHandler(SessionStage &session,
                                         const std::string &name)
{
  auto key = std::make_tuple(session.getBackend(), name);
  auto declIt = _declHandlers.find(key);
  if(declIt != _declHandlers.cend()) {
    return true;
  }
  auto stmtIt = _stmtHandlers.find(key);
  return stmtIt != _stmtHandlers.cend();
}
}