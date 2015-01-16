/**
 * @file examples/megaclient.cpp
 * @brief Sample application, interactive GNU Readline CLI
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include "mega.h"
#include "megacli.h"

using namespace mega;

MegaClient* client;

// new account signup e-mail address and name
static string signupemail, signupname;

// signup code being confirmed
static string signupcode;

// signup password challenge and encrypted master key
static byte signuppwchallenge[SymmCipher::KEYLENGTH], signupencryptedmasterkey[SymmCipher::KEYLENGTH];

// local console
Console* console;

// loading progress of lengthy API responses
int responseprogress = -1;

appfile_list appxferq[2];

static const char* accesslevels[] =
{ "read-only", "read/write", "full access" };

unsigned state = 0;

const char* errorstring(error e)
{
    switch (e)
    {
        case API_OK:
            return "No error";
        case API_EINTERNAL:
            return "Internal error";
        case API_EARGS:
            return "Invalid argument";
        case API_EAGAIN:
            return "Request failed, retrying";
        case API_ERATELIMIT:
            return "Rate limit exceeded";
        case API_EFAILED:
            return "Transfer failed";
        case API_ETOOMANY:
            return "Too many concurrent connections or transfers";
        case API_ERANGE:
            return "Out of range";
        case API_EEXPIRED:
            return "Expired";
        case API_ENOENT:
            return "Not found";
        case API_ECIRCULAR:
            return "Circular linkage detected";
        case API_EACCESS:
            return "Access denied";
        case API_EEXIST:
            return "Already exists";
        case API_EINCOMPLETE:
            return "Incomplete";
        case API_EKEY:
            return "Invalid key/integrity check failed";
        case API_ESID:
            return "Bad session ID";
        case API_EBLOCKED:
            return "Blocked";
        case API_EOVERQUOTA:
            return "Over quota";
        case API_ETEMPUNAVAIL:
            return "Temporarily not available";
        case API_ETOOMANYCONNECTIONS:
            return "Connection overflow";
        case API_EWRITE:
            return "Write error";
        case API_EREAD:
            return "Read error";
        case API_EAPPKEY:
            return "Invalid application key";
        default:
            return "Unknown error";
    }
}

AppFile::AppFile()
{
    static int nextseqno;

    seqno = ++nextseqno;
}

// transfer start
void AppFilePut::start()
{
}

void AppFileGet::start()
{
}

// returns true to effect a retry, false to effect a failure
bool AppFile::failed(error e)
{
    return e != API_EKEY && e != API_EBLOCKED && transfer->failcount < 10;
}

// transfer completion
void AppFileGet::completed(Transfer*, LocalNode*)
{
    // (at this time, the file has already been placed in the final location)
    delete this;
}

void AppFilePut::completed(Transfer* t, LocalNode*)
{
    // perform standard completion (place node in user filesystem etc.)
    File::completed(t, NULL);

    delete this;
}

AppFileGet::~AppFileGet()
{
    appxferq[GET].erase(appxfer_it);
}

AppFilePut::~AppFilePut()
{
    appxferq[PUT].erase(appxfer_it);
}

void AppFilePut::displayname(string* dname)
{
    *dname = localname;
    transfer->client->fsaccess->local2name(dname);
}

// transfer progress callback
void AppFile::progress()
{
}

static void displaytransferdetails(Transfer* t, const char* action)
{
    string name;

    for (file_list::iterator it = t->files.begin(); it != t->files.end(); it++)
    {
        if (it != t->files.begin())
        {
            cout << "/";
        }

        (*it)->displayname(&name);
        cout << name;
    }

    cout << ": " << (t->type == GET ? "Incoming" : "Outgoing") << " file transfer " << action;
}

// a new transfer was added
void DemoApp::transfer_added(Transfer* t)
{
}

// a queued transfer was removed
void DemoApp::transfer_removed(Transfer* t)
{
    displaytransferdetails(t, "removed\n");
}

void DemoApp::transfer_update(Transfer* t)
{
    // (this is handled in the prompt logic)
}

void DemoApp::transfer_failed(Transfer* t, error e)
{
    displaytransferdetails(t, "failed (");
    cout << errorstring(e) << ")" << endl;
}

void DemoApp::transfer_limit(Transfer *t)
{
    displaytransferdetails(t, "bandwidth limit reached\n");
}

void DemoApp::transfer_complete(Transfer* t)
{
    displaytransferdetails(t, "completed, ");

    if (t->slot)
    {
        cout << t->slot->progressreported * 10 / (1024 * (Waiter::ds - t->slot->starttime + 1)) << " KB/s" << endl;
    }
    else
    {
        cout << "delayed" << endl;
    }
}

// transfer about to start - make final preparations (determine localfilename, create thumbnail for image upload)
void DemoApp::transfer_prepare(Transfer* t)
{
    displaytransferdetails(t, "starting\n");

    if (t->type == GET)
    {
        // only set localfilename if the engine has not already done so
        if (!t->localfilename.size())
        {
            client->fsaccess->tmpnamelocal(&t->localfilename);
        }
    }
}

#ifdef ENABLE_SYNC
static void syncstat(Sync* sync)
{
    cout << ", local data in this sync: " << sync->localbytes << " byte(s) in " << sync->localnodes[FILENODE]
         << " file(s) and " << sync->localnodes[FOLDERNODE] << " folder(s)" << endl;
}

void DemoApp::syncupdate_state(Sync*, syncstate_t newstate)
{
    switch (newstate)
    {
        case SYNC_ACTIVE:
            cout << "Sync is now active" << endl;
            break;

        case SYNC_FAILED:
            cout << "Sync failed." << endl;

        default:
            ;
    }
}

void DemoApp::syncupdate_scanning(bool active)
{
    if (active)
    {
        cout << "Sync - scanning files and folders" << endl;
    }
    else
    {
        cout << "Sync - scan completed" << endl;
    }
}

// sync update callbacks are for informational purposes only and must not change or delete the sync itself
void DemoApp::syncupdate_local_folder_addition(Sync* sync, LocalNode *, const char* path)
{
    cout << "Sync - local folder addition detected: " << path;
    syncstat(sync);
}

void DemoApp::syncupdate_local_folder_deletion(Sync* sync, LocalNode *localNode)
{
    cout << "Sync - local folder deletion detected: " << localNode->name;
    syncstat(sync);
}

void DemoApp::syncupdate_local_file_addition(Sync* sync, LocalNode *, const char* path)
{
    cout << "Sync - local file addition detected: " << path;
    syncstat(sync);
}

void DemoApp::syncupdate_local_file_deletion(Sync* sync, LocalNode *localNode)
{
    cout << "Sync - local file deletion detected: " << localNode->name;
    syncstat(sync);
}

void DemoApp::syncupdate_local_file_change(Sync* sync, LocalNode *, const char* path)
{
    cout << "Sync - local file change detected: " << path;
    syncstat(sync);
}

void DemoApp::syncupdate_local_move(Sync*, LocalNode *localNode, const char* path)
{
    cout << "Sync - local rename/move " << localNode->name << " -> " << path << endl;
}

void DemoApp::syncupdate_local_lockretry(bool locked)
{
    if (locked)
    {
        cout << "Sync - waiting for local filesystem lock" << endl;
    }
    else
    {
        cout << "Sync - local filesystem lock issue resolved, continuing..." << endl;
    }
}

void DemoApp::syncupdate_remote_move(Sync *, Node *n, Node *prevparent)
{
    cout << "Sync - remote move " << n->displayname() << ": " << (prevparent ? prevparent->displayname() : "?") <<
            " -> " << (n->parent ? n->parent->displayname() : "?") << endl;
}

void DemoApp::syncupdate_remote_rename(Sync *, Node *n, const char *prevname)
{
    cout << "Sync - remote rename " << prevname << " -> " <<  n->displayname() << endl;
}

void DemoApp::syncupdate_remote_folder_addition(Sync *, Node* n)
{
    cout << "Sync - remote folder addition detected " << n->displayname() << endl;
}

void DemoApp::syncupdate_remote_file_addition(Sync *, Node* n)
{
    cout << "Sync - remote file addition detected " << n->displayname() << endl;
}

void DemoApp::syncupdate_remote_folder_deletion(Sync *, Node* n)
{
    cout << "Sync - remote folder deletion detected " << n->displayname() << endl;
}

void DemoApp::syncupdate_remote_file_deletion(Sync *, Node* n)
{
    cout << "Sync - remote file deletion detected " << n->displayname() << endl;
}

void DemoApp::syncupdate_get(Sync*, Node *, const char* path)
{
    cout << "Sync - requesting file " << path << endl;
}

void DemoApp::syncupdate_put(Sync*, LocalNode *, const char* path)
{
    cout << "Sync - sending file " << path << endl;
}

void DemoApp::syncupdate_remote_copy(Sync*, const char* name)
{
    cout << "Sync - creating remote file " << name << " by copying existing remote file" << endl;
}

static const char* treestatename(treestate_t ts)
{
    switch (ts)
    {
        case TREESTATE_NONE:
            return "None/Undefined";
        case TREESTATE_SYNCED:
            return "Synced";
        case TREESTATE_PENDING:
            return "Pending";
        case TREESTATE_SYNCING:
            return "Syncing";
    }

    return "UNKNOWN";
}

void DemoApp::syncupdate_treestate(LocalNode* l)
{
    cout << "Sync - state change of node " << l->name << " to " << treestatename(l->ts) << endl;
}

// generic name filter
// FIXME: configurable regexps
static bool is_syncable(const char* name)
{
    return *name != '.' && *name != '~' && strcmp(name, "Thumbs.db") && strcmp(name, "desktop.ini");
}

// determines whether remote node should be synced
bool DemoApp::sync_syncable(Node* n)
{
    return is_syncable(n->displayname());
}

// determines whether local file should be synced
bool DemoApp::sync_syncable(const char* name, string* localpath, string* localname)
{
    return is_syncable(name);
}
#endif

AppFileGet::AppFileGet(Node* n, handle ch, byte* cfilekey, m_off_t csize, m_time_t cmtime, string* cfilename,
                       string* cfingerprint)
{
    if (n)
    {
        h = n->nodehandle;
        hprivate = true;

        *(FileFingerprint*) this = *n;
        name = n->displayname();
    }
    else
    {
        h = ch;
        memcpy(filekey, cfilekey, sizeof filekey);
        hprivate = false;

        size = csize;
        mtime = cmtime;

        if (!cfingerprint->size() || !unserializefingerprint(cfingerprint))
        {
            memcpy(crc, filekey, sizeof crc);
        }

        name = *cfilename;
    }

    localname = name;
    client->fsaccess->name2local(&localname);
}

AppFilePut::AppFilePut(string* clocalname, handle ch, const char* ctargetuser)
{
    // this assumes that the local OS uses an ASCII path separator, which should be true for most
    string separator = client->fsaccess->localseparator;

    // full local path
    localname = *clocalname;

    // target parent node
    h = ch;

    // target user
    targetuser = ctargetuser;

    // erase path component
    name = *clocalname;
    client->fsaccess->local2name(&name);
    client->fsaccess->local2name(&separator);

    name.erase(0, name.find_last_of(*separator.c_str()) + 1);
}

// user addition/update (users never get deleted)
void DemoApp::users_updated(User** u, int count)
{
    if (count == 1)
    {
        cout << "1 user received or updated" << endl;
    }
    else
    {
        cout << count << " users received or updated" << endl;
    }
    
    state = 1;
}

void DemoApp::setattr_result(handle, error e)
{
    if (e)
    {
        cout << "Node attribute update failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::rename_result(handle, error e)
{
    if (e)
    {
        cout << "Node move failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::unlink_result(handle, error e)
{
    if (e)
    {
        cout << "Node deletion failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::fetchnodes_result(error e)
{
    if (e)
    {
        cout << "File/folder retrieval failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::putnodes_result(error e, targettype_t t, NewNode* nn)
{
    if (t == USER_HANDLE)
    {
        delete[] nn;

        if (!e)
        {
            cout << "Success." << endl;
        }
    }

    if (e)
    {
        cout << "Node addition failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::share_result(error e)
{
    if (e)
    {
        cout << "Share creation/modification request failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::share_result(int, error e)
{
    if (e)
    {
        cout << "Share creation/modification failed (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "Share creation/modification succeeded" << endl;
    }
}

void DemoApp::fa_complete(Node* n, fatype type, const char* data, uint32_t len)
{
    cout << "Got attribute of type " << type << " (" << len << " byte(s)) for " << n->displayname() << endl;
}

int DemoApp::fa_failed(handle, fatype type, int retries)
{
    cout << "File attribute retrieval of type " << type << " failed (retries: " << retries << ")" << endl;

    return retries > 2;
}

void DemoApp::putfa_result(handle, fatype, error e)
{
    if (e)
    {
        cout << "File attribute attachment failed (" << errorstring(e) << ")" << endl;
    }
}

void DemoApp::invite_result(error e)
{
    if (e)
    {
        cout << "Invitation failed (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "Success." << endl;
    }
}

void DemoApp::putua_result(error e)
{
    if (e)
    {
        cout << "User attribute update failed (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "Success." << endl;
    }
}

void DemoApp::getua_result(error e)
{
    cout << "User attribute retrieval failed (" << errorstring(e) << ")" << endl;
}

void DemoApp::getua_result(byte* data, unsigned l)
{
    cout << "Received " << l << " byte(s) of user attribute: ";
    fwrite(data, 1, l, stdout);
    cout << endl;
}

void DemoApp::notify_retry(dstime dsdelta)
{
    if (dsdelta)
    {
        cout << "API request failed, retrying in " << dsdelta * 100 << " ms - Use 'retry' to retry immediately..."
             << endl;
    }
    else
    {
        cout << "Retried API request completed" << endl;
    }
}

static AccountDetails account;

static handle cwd = UNDEF;

static const char* rootnodenames[] =
{ "ROOT", "INBOX", "RUBBISH" };

static void nodestats(int* c, const char* action)
{
    if (c[FILENODE])
    {
        cout << c[FILENODE] << ((c[FILENODE] == 1) ? " file" : " files");
    }
    if (c[FILENODE] && c[FOLDERNODE])
    {
        cout << " and ";
    }
    if (c[FOLDERNODE])
    {
        cout << c[FOLDERNODE] << ((c[FOLDERNODE] == 1) ? " folder" : " folders");
    }

    if (c[FILENODE] || c[FOLDERNODE])
    {
        cout << " " << action << endl;
    }
}

static void listnodeshares(Node* n)
{
    if(n->outshares)
    {
        for (share_map::iterator it = n->outshares->begin(); it != n->outshares->end(); it++)
        {
            cout << "\t" << n->displayname();

            if (it->first)
            {
                cout << ", shared with " << it->second->user->email << " (" << accesslevels[it->second->access] << ")"
                     << endl;
            }
            else
            {
                cout << ", shared as exported folder link" << endl;
            }
        }
    }
}

void TreeProcListOutShares::proc(MegaClient*, Node* n)
{
    listnodeshares(n);
}

static void nodepath(handle h, string* path)
{
    path->clear();

    if (h == client->rootnodes[0])
    {
        *path = "/";
        return;
    }

    Node* n = client->nodebyhandle(h);

    while (n)
    {
        switch (n->type)
        {
            case FOLDERNODE:
                path->insert(0, n->displayname());

                if (n->inshare)
                {
                    path->insert(0, ":");
                    if (n->inshare->user)
                    {
                        path->insert(0, n->inshare->user->email);
                    }
                    else
                    {
                        path->insert(0, "UNKNOWN");
                    }
                    return;
                }
                break;

            case INCOMINGNODE:
                path->insert(0, "//in");
                return;

            case ROOTNODE:
                return;

            case RUBBISHNODE:
                path->insert(0, "//bin");
                return;

            case TYPE_UNKNOWN:
            case FILENODE:
                path->insert(0, n->displayname());
        }

        path->insert(0, "/");

        n = n->parent;
    }
}

TreeProcCopy::TreeProcCopy()
{
    nn = NULL;
    nc = 0;
}

void TreeProcCopy::allocnodes()
{
    nn = new NewNode[nc];
}

TreeProcCopy::~TreeProcCopy()
{
    delete[] nn;
}

// determine node tree size (nn = NULL) or write node tree to new nodes array
void TreeProcCopy::proc(MegaClient* client, Node* n)
{
    if (nn)
    {
        string attrstring;
        SymmCipher key;
        NewNode* t = nn + --nc;

        // copy node
        t->source = NEW_NODE;
        t->type = n->type;
        t->nodehandle = n->nodehandle;
        t->parenthandle = n->parent->nodehandle;

        // copy key (if file) or generate new key (if folder)
        if (n->type == FILENODE)
        {
            t->nodekey = n->nodekey;
        }
        else
        {
            byte buf[FOLDERNODEKEYLENGTH];
            PrnGen::genblock(buf, sizeof buf);
            t->nodekey.assign((char*) buf, FOLDERNODEKEYLENGTH);
        }

        key.setkey((const byte*) t->nodekey.data(), n->type);

        n->attrs.getjson(&attrstring);
        t->attrstring = new string;
        client->makeattr(&key, t->attrstring, attrstring.c_str());
    }
    else
    {
        nc++;
    }
}

int loadfile(string* name, string* data)
{
    FileAccess* fa = client->fsaccess->newfileaccess();

    if (fa->fopen(name, 1, 0))
    {
        data->resize(fa->size);
        fa->fread(data, data->size(), 0, 0);
        delete fa;

        return 1;
    }

    delete fa;

    return 0;
}

void xferq(direction_t d, int cancel)
{
    string name;

    for (appfile_list::iterator it = appxferq[d].begin(); it != appxferq[d].end(); )
    {
        if (cancel < 0 || cancel == (*it)->seqno)
        {
            (*it)->displayname(&name);

            cout << (*it)->seqno << ": " << name;

            if (d == PUT)
            {
                AppFilePut* f = (AppFilePut*) *it;

                cout << " -> ";

                if (f->targetuser.size())
                {
                    cout << f->targetuser << ":";
                }
                else
                {
                    string path;
                    nodepath(f->h, &path);
                    cout << path;
                }
            }

            if ((*it)->transfer && (*it)->transfer->slot)
            {
                cout << " [ACTIVE]";
            }
            cout << endl;

            if (cancel >= 0)
            {
                cout << "Canceling..." << endl;

                if ((*it)->transfer)
                {
                    client->stopxfer(*it);
                }
                delete *it++;
            }
            else
            {
                it++;
            }
        }
        else
        {
            it++;
        }
    }
}

// password change-related state information
static byte pwkey[SymmCipher::KEYLENGTH];

// callback for non-EAGAIN request-level errors
// in most cases, retrying is futile, so the application exits
// this can occur e.g. with syntactically malformed requests (due to a bug), an invalid application key
void DemoApp::request_error(error e)
{
    if (e == API_ESID)
    {
        cout << "Invalid or expired session, logging out..." << endl;
        client->logout();
        return;
    }

    cout << "FATAL: Request failed (" << errorstring(e) << "), exiting" << endl;

    delete console;
    exit(0);
}

void DemoApp::request_response_progress(m_off_t current, m_off_t total)
{
    if (total > 0)
    {
        responseprogress = current * 100 / total;
    }
    else
    {
        responseprogress = -1;
    }
}

// login result
void DemoApp::login_result(error e)
{
    if (e)
    {
        cout << "Login failed: " << errorstring(e) << endl;
    }
    else
    {
        cout << "Login successful, retrieving account..." << endl;
        client->fetchnodes();
    }
}

// ephemeral session result
void DemoApp::ephemeral_result(error e)
{
    if (e)
    {
        cout << "Ephemeral session error (" << errorstring(e) << ")" << endl;
    }
}

// signup link send request result
void DemoApp::sendsignuplink_result(error e)
{
    if (e)
    {
        cout << "Unable to send signup link (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "Thank you. Please check your e-mail and enter the command signup followed by the confirmation link." << endl;
    }
}

// signup link query result
void DemoApp::querysignuplink_result(handle uh, const char* email, const char* name, const byte* pwc, const byte* kc,
                                     const byte* c, size_t len)
{
    cout << "Ready to confirm user account " << email << " (" << name << ") - enter confirm to execute." << endl;

    signupemail = email;
    signupcode.assign((char*) c, len);
    memcpy(signuppwchallenge, pwc, sizeof signuppwchallenge);
    memcpy(signupencryptedmasterkey, pwc, sizeof signupencryptedmasterkey);
}

// signup link query failed
void DemoApp::querysignuplink_result(error e)
{
    cout << "Signuplink confirmation failed (" << errorstring(e) << ")" << endl;
}

// signup link (account e-mail) confirmation result
void DemoApp::confirmsignuplink_result(error e)
{
    if (e)
    {
        cout << "Signuplink confirmation failed (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "Signup confirmed, logging in..." << endl;
        client->login(signupemail.c_str(), pwkey);
    }
}

// asymmetric keypair configuration result
void DemoApp::setkeypair_result(error e)
{
    if (e)
    {
        cout << "RSA keypair setup failed (" << errorstring(e) << ")" << endl;
    }
    else
    {
        cout << "RSA keypair added. Account setup complete." << endl;
    }
}

void DemoApp::ephemeral_result(handle uh, const byte* pw)
{
    char buf[SymmCipher::KEYLENGTH * 4 / 3 + 3];

    cout << "Ephemeral session established, session ID: ";
    Base64::btoa((byte*) &uh, sizeof uh, buf);
    cout << buf << "#";
    Base64::btoa(pw, SymmCipher::KEYLENGTH, buf);
    cout << buf << endl;

    client->fetchnodes();
}

// password change result
void DemoApp::changepw_result(error e)
{
    if (e)
    {
        cout << "Password update failed: " << errorstring(e) << endl;
    }
    else
    {
        cout << "Password updated." << endl;
    }
}

// node export failed
void DemoApp::exportnode_result(error e)
{
    if (e)
    {
        cout << "Export failed: " << errorstring(e) << endl;
    }
}

void DemoApp::exportnode_result(handle h, handle ph)
{
    Node* n;

    if ((n = client->nodebyhandle(h)))
    {
        string path;
        char node[9];
        char key[FILENODEKEYLENGTH * 4 / 3 + 3];

        nodepath(h, &path);

        cout << "Exported " << path << ": ";

        Base64::btoa((byte*) &ph, MegaClient::NODEHANDLE, node);

        // the key
        if (n->type == FILENODE)
        {
            Base64::btoa((const byte*) n->nodekey.data(), FILENODEKEYLENGTH, key);
        }
        else if (n->sharekey)
        {
            Base64::btoa(n->sharekey->key, FOLDERNODEKEYLENGTH, key);
        }
        else
        {
            cout << "No key available for exported folder" << endl;
            return;
        }

        cout << "https://mega.co.nz/#" << (n->type ? "F" : "") << "!" << node << "!" << key << endl;
    }
    else
    {
        cout << "Exported node no longer available" << endl;
    }
}

// the requested link could not be opened
void DemoApp::openfilelink_result(error e)
{
    if (e)
    {
        cout << "Failed to open link: " << errorstring(e) << endl;
    }
}

// the requested link was opened successfully - import to cwd
void DemoApp::openfilelink_result(handle ph, const byte* key, m_off_t size,
                                  string* a, string* fa, int)
{
    Node* n;

    if (client->loggedin() != NOTLOGGEDIN && (n = client->nodebyhandle(cwd)))
    {
        NewNode* newnode = new NewNode[1];

        // set up new node as folder node
        newnode->source = NEW_PUBLIC;
        newnode->type = FILENODE;
        newnode->nodehandle = ph;
        newnode->parenthandle = UNDEF;

        newnode->nodekey.assign((char*)key, FILENODEKEYLENGTH);

        newnode->attrstring = new string(*a);

        client->putnodes(n->nodehandle, newnode, 1);
    }
    else
    {
        cout << "Need to be logged in to import file links." << endl;
    }
}

void DemoApp::checkfile_result(handle h, error e)
{
    cout << "Link check failed: " << errorstring(e) << endl;
}

void DemoApp::checkfile_result(handle h, error e, byte* filekey, m_off_t size, m_time_t ts, m_time_t tm, string* filename,
                               string* fingerprint, string* fileattrstring)
{
    cout << "Name: " << *filename << ", size: " << size;

    if (fingerprint->size())
    {
        cout << ", fingerprint available";
    }

    if (fileattrstring->size())
    {
        cout << ", has attributes";
    }

    cout << endl;

    if (e)
    {
        cout << "Not available: " << errorstring(e) << endl;
    }
    else
    {
        cout << "Initiating download..." << endl;

        AppFileGet* f = new AppFileGet(NULL, h, filekey, size, tm, filename, fingerprint);
        f->appxfer_it = appxferq[GET].insert(appxferq[GET].end(), f);
        client->startxfer(GET, f);
    }
}

bool DemoApp::pread_data(byte* data, m_off_t len, m_off_t pos, void* appdata)
{
    cout << "Received " << len << " partial read byte(s) at position " << pos << ": ";
    fwrite(data, 1, len, stdout);
    cout << endl;

    return true;
}

dstime DemoApp::pread_failure(error e, int retry, void* appdata)
{
    if (retry < 5)
    {
        cout << "Retrying read (" << errorstring(e) << ", attempt #" << retry << ")" << endl;
        return (dstime)(retry*10);
    }
    else
    {
        cout << "Too many failures (" << errorstring(e) << "), giving up" << endl;
        return ~(dstime)0;
    }
}

// reload needed
void DemoApp::reload(const char* reason)
{
    cout << "Reload suggested (" << reason << ") - use 'reload' to trigger" << endl;
}

// reload initiated
void DemoApp::clearing()
{
    LOG_debug << "Clearing all nodes/users...";
}

// nodes have been modified
// (nodes with their removed flag set will be deleted immediately after returning from this call,
// at which point their pointers will become invalid at that point.)
void DemoApp::nodes_updated(Node** n, int count)
{
    int c[2][6] = { { 0 } };

    if (n)
    {
        while (count--)
        {
            if ((*n)->type < 6)
            {
                c[!(*n)->changed.removed][(*n)->type]++;
                n++;
            }
        }
    }
    else
    {
        for (node_map::iterator it = client->nodes.begin(); it != client->nodes.end(); it++)
        {
            if (it->second->type < 6)
            {
                c[1][it->second->type]++;
            }
        }
    }

    nodestats(c[1], "added or updated");
    nodestats(c[0], "removed");

    if (ISUNDEF(cwd))
    {
        cwd = client->rootnodes[0];
    }
    
    state = 2;
}

// nodes now (almost) current, i.e. no server-client notifications pending
void DemoApp::nodes_current()
{
    LOG_debug << "Nodes current.";
}

void DemoApp::enumeratequotaitems_result(handle, unsigned, unsigned, unsigned, unsigned, unsigned, const char*)
{
    // FIXME: implement
}

void DemoApp::enumeratequotaitems_result(error)
{
    // FIXME: implement
}

void DemoApp::additem_result(error)
{
    // FIXME: implement
}

void DemoApp::checkout_result(error)
{
    // FIXME: implement
}

void DemoApp::checkout_result(const char*)
{
    // FIXME: implement
}

// display account details/history
void DemoApp::account_details(AccountDetails* ad, bool storage, bool transfer, bool pro, bool purchases,
                              bool transactions, bool sessions)
{
    char timebuf[32], timebuf2[32];

    if (storage)
    {
        cout << "\tAvailable storage: " << ad->storage_max << " byte(s)" << endl;

        for (unsigned i = 0; i < sizeof rootnodenames/sizeof *rootnodenames; i++)
        {
            NodeStorage* ns = &ad->storage[client->rootnodes[i]];

            cout << "\t\tIn " << rootnodenames[i] << ": " << ns->bytes << " byte(s) in " << ns->files << " file(s) and " << ns->folders << " folder(s)" << endl;
        }
    }

    if (transfer)
    {
        if (ad->transfer_max)
        {
            cout << "\tTransfer in progress: " << ad->transfer_own_reserved << "/" << ad->transfer_srv_reserved << endl;
            cout << "\tTransfer completed: " << ad->transfer_own_used << "/" << ad->transfer_srv_used << " of "
                 << ad->transfer_max << " ("
                 << (100 * (ad->transfer_own_used + ad->transfer_srv_used) / ad->transfer_max) << "%)" << endl;
            cout << "\tServing bandwidth ratio: " << ad->srv_ratio << "%" << endl;
        }

        if (ad->transfer_hist_starttime)
        {
            time_t t = time(NULL) - ad->transfer_hist_starttime;

            cout << "\tTransfer history:\n";

            for (unsigned i = 0; i < ad->transfer_hist.size(); i++)
            {
                t -= ad->transfer_hist_interval;
                cout << "\t\t" << t;
                if (t < ad->transfer_hist_interval)
                {
                    cout << " second(s) ago until now: ";
                }
                else
                {
                    cout << "-" << t - ad->transfer_hist_interval << " second(s) ago: ";
                }
                cout << ad->transfer_hist[i] << " byte(s)" << endl;
            }
        }

        if (ad->transfer_limit)
        {
            cout << "Per-IP transfer limit: " << ad->transfer_limit << endl;
        }
    }

    if (pro)
    {
        cout << "\tPro level: " << ad->pro_level << endl;
        cout << "\tSubscription type: " << ad->subscription_type << endl;
        cout << "\tAccount balance:" << endl;

        for (vector<AccountBalance>::iterator it = ad->balances.begin(); it != ad->balances.end(); it++)
        {
            printf("\tBalance: %.3s %.02f\n", it->currency, it->amount);
        }
    }

    if (purchases)
    {
        cout << "Purchase history:" << endl;

        for (vector<AccountPurchase>::iterator it = ad->purchases.begin(); it != ad->purchases.end(); it++)
        {
            time_t ts = it->timestamp;
            strftime(timebuf, sizeof timebuf, "%c", localtime(&ts));
            printf("\tID: %.11s Time: %s Amount: %.3s %.02f Payment method: %d\n", it->handle, timebuf, it->currency,
                   it->amount, it->method);
        }
    }

    if (transactions)
    {
        cout << "Transaction history:" << endl;

        for (vector<AccountTransaction>::iterator it = ad->transactions.begin(); it != ad->transactions.end(); it++)
        {
            time_t ts = it->timestamp;
            strftime(timebuf, sizeof timebuf, "%c", localtime(&ts));
            printf("\tID: %.11s Time: %s Delta: %.3s %.02f\n", it->handle, timebuf, it->currency, it->delta);
        }
    }

    if (sessions)
    {
        cout << "Session history:" << endl;

        for (vector<AccountSession>::iterator it = ad->sessions.begin(); it != ad->sessions.end(); it++)
        {
            time_t ts = it->timestamp;
            strftime(timebuf, sizeof timebuf, "%c", localtime(&ts));
            ts = it->mru;
            strftime(timebuf2, sizeof timebuf, "%c", localtime(&ts));
            printf("\tSession start: %s Most recent activity: %s IP: %s Country: %.2s User-Agent: %s\n",
                    timebuf, timebuf2, it->ip.c_str(), it->country, it->useragent.c_str());
        }
    }
}

// account details could not be retrieved
void DemoApp::account_details(AccountDetails* ad, error e)
{
    if (e)
    {
        cout << "Account details retrieval failed (" << errorstring(e) << ")" << endl;
    }
}

// user attribute update notification
void DemoApp::userattr_update(User* u, int priv, const char* n)
{
    cout << "Notification: User " << u->email << " -" << (priv ? " private" : "") << " attribute "
          << n << " added or updated" << endl;
}

void loginAndUploadFile(const char* User, const char* Password, const char* FilePath)
{
    // instantiate app components: the callback processor (DemoApp),
    // the HTTP I/O engine (WinHttpIO) and the MegaClient itself
    client = new MegaClient(new DemoApp,
                            new CONSOLE_WAIT_CLASS,
                            new HTTPIO_CLASS,
                            new FSACCESS_CLASS,
                            NULL,
                            NULL,
                            "CameraPi",
                            "megaCameraPi/" TOSTRING(MEGA_MAJOR_VERSION)
                            "." TOSTRING(MEGA_MINOR_VERSION)
                            "." TOSTRING(MEGA_MICRO_VERSION));
    
    byte my_pwkey[SymmCipher::KEYLENGTH];
    client->pw_key(Password, my_pwkey);
    client->login(User, my_pwkey);
    client->exec();
    state = 0;
    while(state != 1)
    {
        if (client->wait())
        {
            client->exec();
            if (state == 1) break;
        }
    }
    
    if (client->loggedin() == NOTLOGGEDIN) return;
    
    /////////////////////////////
    // Start Upload
    AppFile* f;
    handle target = cwd;
    string targetuser;
    string localname;
    string name;
    nodetype_t type;
    
    
    if (client->loggedin() == NOTLOGGEDIN && !targetuser.size())
    {
        cout << "Not logged in." << endl;
        
        return;
    }
    
    std::string str_file_path(FilePath);
    client->fsaccess->path2local(&str_file_path, &localname);
    
    DirAccess* da = client->fsaccess->newdiraccess();
    
    if (da->dopen(&localname, NULL, true))
    {
        while (da->dnext(NULL, &localname, true, &type))
        {
            client->fsaccess->local2path(&localname, &name);
            cout << "Queueing " << name << "..." << endl;
            
            if (type == FILENODE)
            {
                f = new AppFilePut(&localname, target, targetuser.c_str());
                f->appxfer_it = appxferq[PUT].insert(appxferq[PUT].end(), f);
                client->startxfer(PUT, f);
            }
        }
    }
    
    delete da;
    ////////////////////////////
    
    while (true)
    {
        if (client->wait())
        {
            client->exec();
            if (state == 2) break;
        }
    }
}

// int main()
// {
//     loginAndUploadFile("sunnyfuture@gmail.com", "cxw@2623810", "/Users/future/process.R");
// }
