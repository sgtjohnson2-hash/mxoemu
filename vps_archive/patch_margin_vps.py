with open('/home/ubuntu/mxoemu/Server/Reality/Source/MarginSocket.cpp', 'r') as f:
    content = f.read()

target = """\tif (charId == 0)
\t{
\t\tPreparedStatement selStmt("SELECT `charId`, `handle` FROM `characters` WHERE `userId` = ?0 ORDER BY `charId` DESC LIMIT 1");
\t\tselStmt.SetUInt32(0, m_userId);
\t\tscoped_ptr<QueryResult> res(sDatabase.QueryPrepared(&selStmt));
\t\tif (res)
\t\t{
\t\t\tField* f = res->Fetch();
\t\t\tcharId = f[0].GetUInt64();
\t\t\tm_charName = f[1].GetString();
\t\t}
\t}

\tif (charId == 0)
\t{
\t\tERROR_LOG("MS_CreateCharacterRequest: No character found for user, cannot save RSI.");
\t\treturn;
\t}"""

replacement = """\tstring handleToUse = m_charName;
\tif (handleToUse.empty())
\t\thandleToUse = firstName;
\tif (handleToUse.empty())
\t\thandleToUse = m_username;
\tm_charName = handleToUse;

\tif (charId == 0)
\t{
\t\tPreparedStatement selStmt("SELECT `charId`, `handle` FROM `characters` WHERE `userId` = ?0 AND `handle` = ?1 ORDER BY `charId` DESC LIMIT 1");
\t\tselStmt.SetUInt32(0, m_userId);
\t\tselStmt.SetString(1, handleToUse);
\t\tscoped_ptr<QueryResult> res(sDatabase.QueryPrepared(&selStmt));
\t\tif (res)
\t\t{
\t\t\tField* f = res->Fetch();
\t\t\tcharId = f[0].GetUInt64();
\t\t}
\t}

\tif (charId == 0)
\t{
\t\tPreparedStatement selAnyStmt("SELECT `charId`, `handle` FROM `characters` WHERE `userId` = ?0 ORDER BY `charId` DESC LIMIT 1");
\t\tselAnyStmt.SetUInt32(0, m_userId);
\t\tscoped_ptr<QueryResult> resAny(sDatabase.QueryPrepared(&selAnyStmt));
\t\tif (resAny)
\t\t{
\t\t\tField* f = resAny->Fetch();
\t\t\tcharId = f[0].GetUInt64();
\t\t\tm_charName = f[1].GetString();
\t\t\thandleToUse = m_charName;
\t\t}
\t}

\tif (charId == 0)
\t{
\t\tPreparedStatement checkOther("SELECT `charId` FROM `characters` WHERE `handle` = ?0 LIMIT 1");
\t\tcheckOther.SetString(0, handleToUse);
\t\tscoped_ptr<QueryResult> otherRes(sDatabase.QueryPrepared(&checkOther));
\t\tif (otherRes)
\t\t{
\t\t\thandleToUse = handleToUse + "_" + std::to_string(m_userId);
\t\t\tm_charName = handleToUse;
\t\t}

\t\tuint32 profId = (profession > 0) ? profession : 2;
\t\tPreparedStatement insStmt("INSERT INTO `characters` (`userId`, `worldId`, `status`, `handle`, `firstName`, `lastName`, `background`, `x`, `y`, `z`, `rot`, `healthC`, `healthM`, `innerStrC`, `innerStrM`, `level`, `profession`, `alignment`, `pvpflag`, `exp`, `cash`, `district`, `adminFlags`) "
\t\t\t"VALUES (?0, 1, 0, ?1, ?2, ?3, ?4, 16802.3, 495.0, 3237.01, 0.0245437, 500, 500, 200, 200, 50, ?5, 0, 0, 1000000000, 10000, 1, 0)");
\t\tinsStmt.SetUInt32(0, m_userId);
\t\tinsStmt.SetString(1, handleToUse);
\t\tinsStmt.SetString(2, firstName);
\t\tinsStmt.SetString(3, lastName);
\t\tinsStmt.SetString(4, description);
\t\tinsStmt.SetUInt32(5, profId);
\t\tsDatabase.ExecutePrepared(&insStmt);

\t\tPreparedStatement getStmt("SELECT `charId` FROM `characters` WHERE `userId` = ?0 AND `handle` = ?1 ORDER BY `charId` DESC LIMIT 1");
\t\tgetStmt.SetUInt32(0, m_userId);
\t\tgetStmt.SetString(1, handleToUse);
\t\tscoped_ptr<QueryResult> newRes(sDatabase.QueryPrepared(&getStmt));
\t\tif (newRes)
\t\t{
\t\t\tField* f = newRes->Fetch();
\t\t\tcharId = f[0].GetUInt64();
\t\t}
\t\telse
\t\t{
\t\t\tcharId = 9001288;
\t\t}
\t}"""

if target in content:
    content = content.replace(target, replacement)
    with open('/home/ubuntu/mxoemu/Server/Reality/Source/MarginSocket.cpp', 'w') as f:
        f.write(content)
    print("SUCCESS: Patched MarginSocket.cpp")
else:
    print("WARNING: Exact target block not found, searching with normalized line endings...")
    # Normalize \r\n to \n
    content_norm = content.replace('\r\n', '\n')
    target_norm = target.replace('\r\n', '\n')
    if target_norm in content_norm:
        content_norm = content_norm.replace(target_norm, replacement)
        with open('/home/ubuntu/mxoemu/Server/Reality/Source/MarginSocket.cpp', 'w') as f:
            f.write(content_norm)
        print("SUCCESS: Patched MarginSocket.cpp (normalized)")
    else:
        print("ERROR: Target still not found!")
