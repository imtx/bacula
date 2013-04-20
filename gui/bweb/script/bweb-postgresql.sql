-- Require > 7.4, else use createlang command
CREATE PROCEDURAL LANGUAGE plpgsql;

-- --------------------------------------------------
-- Upgrade from 5.0
-- --------------------------------------------------

BEGIN;

-- PG 8.4 drops implicit cast from double to bigint
CREATE FUNCTION SEC_TO_TIME(double precision)
RETURNS interval AS $$
    select date_trunc('second', $1 * interval '1 second');
$$ LANGUAGE SQL;

COMMIT;

CREATE UNIQUE INDEX location_idx ON Location (Location);

-- --------------------------------------------------
-- Upgrade from 2.2
-- --------------------------------------------------

-- New tables for bresto (same as brestore)

CREATE TABLE brestore_knownjobid
(
     JobId integer NOT NULL,
     CONSTRAINT brestore_knownjobid_pkey PRIMARY KEY (JobId)
);

CREATE TABLE brestore_pathhierarchy
(
     PathId integer NOT NULL,
     PPathId integer NOT NULL,
     CONSTRAINT brestore_pathhierarchy_pkey PRIMARY KEY (PathId)
);

CREATE INDEX brestore_pathhierarchy_ppathid 
                          ON brestore_pathhierarchy (PPathId);

CREATE TABLE brestore_pathvisibility
(
      PathId integer NOT NULL,
      JobId integer NOT NULL,
      Size int8 DEFAULT 0,
      Files int4 DEFAULT 0,
      CONSTRAINT brestore_pathvisibility_pkey PRIMARY KEY (JobId, PathId)
);

CREATE INDEX brestore_pathvisibility_jobid
                          ON brestore_pathvisibility (JobId);


-- New job log view

CREATE FUNCTION concat (text, text) RETURNS text AS '
DECLARE
result text;
BEGIN
IF $1 is not null THEN
result := $1 || $2;
END IF;

RETURN result;
END;
' LANGUAGE plpgsql;

CREATE AGGREGATE group_concat(
sfunc = concat,
basetype = text,
stype = text,
initcond = ''
);

BEGIN;
CREATE TABLE bweb_user
(
        userid       serial not null,
        username     text not null,
        use_acl      boolean default false,
        enabled      boolean default true,
        comment      text default '',
        passwd       text default '',
        tpl          text default '',
        primary key (userid)
);
CREATE UNIQUE INDEX bweb_user_idx on bweb_user (username);

CREATE TABLE bweb_role
(
        roleid       serial not null,
        rolename     text not null,
        comment      text default '',
        primary key (roleid)
);
CREATE UNIQUE INDEX bweb_role_idx on bweb_role (rolename);

INSERT INTO bweb_role (rolename) VALUES ('r_user_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_group_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_configure');

INSERT INTO bweb_role (rolename) VALUES ('r_autochanger_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_media_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_location_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_storage_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_delete_job');
INSERT INTO bweb_role (rolename) VALUES ('r_prune');
INSERT INTO bweb_role (rolename) VALUES ('r_purge');

INSERT INTO bweb_role (rolename) VALUES ('r_view_job');
INSERT INTO bweb_role (rolename) VALUES ('r_view_log');
INSERT INTO bweb_role (rolename) VALUES ('r_view_media');
INSERT INTO bweb_role (rolename) VALUES ('r_view_stat');
INSERT INTO bweb_role (rolename) VALUES ('r_view_group');
INSERT INTO bweb_role (rolename) VALUES ('r_view_running_job');

INSERT INTO bweb_role (rolename) VALUES ('r_run_job');
INSERT INTO bweb_role (rolename) VALUES ('r_cancel_job');
INSERT INTO bweb_role (rolename) VALUES ('r_client_status');

CREATE TABLE  bweb_role_member
(
        roleid       integer not null,
        userid       integer not null,
        primary key (roleid, userid)
);

CREATE TABLE  bweb_client_group_acl
(
        client_group_id       integer not null,
        userid                integer not null,
        primary key (client_group_id, userid)
);
COMMIT;
-- --------------------------------------------------
-- Upgrade from 2.0
-- --------------------------------------------------

BEGIN;
-- Manage Client groups in bweb
-- Works with postgresql and mysql5 

CREATE TABLE client_group
(
    client_group_id             serial    not null,
    client_group_name           text      not null,
    comment                     text      default '',
    primary key (client_group_id)
);

CREATE UNIQUE INDEX client_group_idx on client_group (client_group_name);

CREATE TABLE client_group_member
(
    client_group_id                 integer       not null,
    clientid          integer     not null,
    primary key (client_group_id, clientid)
);

CREATE INDEX client_group_member_idx on client_group_member (client_group_id);

COMMIT;

-- --------------------------------------------------
-- End of upgrade from 2.0
-- --------------------------------------------------

--   -- creer un nouveau group
--   
--   INSERT INTO client_group (client_group_name) VALUES ('SIGMA');
--   
--   -- affecter une machine a un group
--   
--   INSERT INTO client_group_member (client_group_id, clientid) 
--          (SELECT client_group_id, 
--                 (SELECT Clientid FROM Client WHERE Name = 'slps0003-fd')
--             FROM client_group 
--            WHERE client_group_name IN ('SIGMA', 'EXPLOITATION', 'MUTUALISE'));
--        
--   
--   -- modifier l'affectation d'une machine
--   
--   DELETE FROM client_group_member 
--         WHERE clientid = (SELECT ClientId FROM Client WHERE Name = 'slps0003-fd')
--   
--   -- supprimer un groupe
--   
--   DELETE FROM client_group_member 
--         WHERE client_group_id = (SELECT client_group_id FROM client_group WHERE client_group_name = 'EXPLOIT')
--   
--   
--   -- afficher tous les clients du group SIGMA
--   
--   SELECT Name FROM Client JOIN client_group_member using (clientid) 
--                           JOIN client_group using (client_group_id)
--    WHERE client_group_name = 'SIGMA';
--   
--   -- afficher tous les groups
--   
--   SELECT client_group_name FROM client_group ORDER BY client_group_name;
--   
--   -- afficher tous les job du group SIGMA hier
--   
--   SELECT JobId, Job.Name, Client.Name, JobStatus, JobErrors
--     FROM Job JOIN Client              USING(ClientId) 
--              JOIN client_group_member USING (ClientId)
--              JOIN client_group        USING (client_group_id)
--     WHERE client_group_name = 'SIGMA'
--       AND Job.StartTime > '2007-03-20'; 
--   
--   -- donne des stats
--   
--   SELECT count(1) AS nb, sum(JobFiles) AS files,
--          sum(JobBytes) AS size, sum(JobErrors) AS joberrors,
--          JobStatus AS jobstatus, client_group_name
--     FROM Job JOIN Client              USING(ClientId) 
--              JOIN client_group_member USING (ClientId)
--              JOIN client_group        USING (client_group_id)
--     WHERE Job.StartTime > '2007-03-20'
--     GROUP BY JobStatus, client_group_name
--   
--   

BEGIN;

CREATE FUNCTION SEC_TO_TIME(timestamp with time zone)
RETURNS timestamp with time zone AS $$
    select date_trunc('second', $1);
$$ LANGUAGE SQL;

CREATE FUNCTION SEC_TO_TIME(bigint)
RETURNS interval AS $$
    select date_trunc('second', $1 * interval '1 second');
$$ LANGUAGE SQL;

CREATE FUNCTION UNIX_TIMESTAMP(timestamp with time zone)
RETURNS double precision AS $$
    select date_part('epoch', $1);
$$ LANGUAGE SQL;

CREATE FUNCTION SEC_TO_INT(interval)
RETURNS double precision AS $$
    select extract(epoch from $1);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION base64_decode_lstat(int4, varchar) RETURNS int8 
IMMUTABLE STRICT AS $$
DECLARE
val int8;
len int8;
b64 varchar(64);
size varchar(64);
i int;
BEGIN
size := split_part($2, ' ', $1);
b64 := 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
val := 0;
len=length(size);
FOR i IN 1..len LOOP
val := val + (strpos(b64, substr(size, i, 1))-1) * (64^(len-i));
END LOOP;
RETURN val;
END;
$$ language 'plpgsql';

COMMIT;


