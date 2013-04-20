<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head>
<title>Bweb - Bacula Web Interface</title>
<link rel="SHORTCUT ICON" href="/bweb/favicon.ico"/>
<script type="text/javascript" language="JavaScript" src="/bweb/natcompare.js"></script>
<script type="text/javascript" language="JavaScript" src="/bweb/nrs_table.js"></script>
<script type="text/javascript" language="JavaScript" src="/bweb/bweb.js"></script>
<link type="text/css" rel="stylesheet" href="/bweb/style.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/kaiska.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/bweb.css"/>
</head>
<body>

<script type="text/javascript" language="JavaScript">
if (navigator.appName == 'Konqueror') {
        alert("Sorry at this moment, bweb works only with mozilla.");
}
if ('Main' == ('_' + '_Main_' + '_')) {
	document.write("<font color='red'>Update your configuration to use the correct tpl directory (You are using devel tpl)</font>");
} 
</script>

<ul id="menu">
 <li><a href="bweb.pl?">Main</a> </li>
 <li><a href="bweb.pl?action=client">Clients</a>
     <ul>
       <li><a href="bweb.pl?action=client">Clients</a> </li>
       <li><a href="bweb.pl?action=groups">Groups</a> </li>
     </ul>
 </li>
 <li style="padding: 0.25em 2em;">Jobs
   <ul> 
     <li><a href="bweb.pl?action=run_job">Defined Jobs</a>
     <li><a href="bweb.pl?action=job_group">Jobs by group</a>
     <li><a href="bweb.pl?action=overview">Jobs overview</a>
     <li><a href="bweb.pl?action=missing">Missing Jobs</a>
     <li><a href="bweb.pl?action=job">Last Jobs</a> </li>
     <li><a href="bweb.pl?action=running">Running Jobs</a>
     <li><a href="bweb.pl?action=next_job">Next Jobs</a> </li>
<!-- <li><a href="bweb.pl?action=restore" title="Launch brestore">Restore</a> </li> -->
     <li><a href="/bweb/bresto.html" title="Try bresto">Web Restore</a> </li>
   </ul>
 </li>
 <li style="padding: 0.25em 2em;">Media
  <ul>
     <li><a href="bweb.pl?action=pool">Pools</a> </li>
     <li><a href="bweb.pl?action=location">Locations</a> </li>
     <li><a href="bweb.pl?action=media">All Media</a><hr></li>
     <li><a href="bweb.pl?action=add_media">Add Media</a><hr></li>
     <li><a href="bweb.pl?action=extern_media">Eject Media</a> </li>
     <li><a href="bweb.pl?action=intern_media">Load Media</a> </li>
  </ul>
 </li>
 <li style="padding: 0.25em 2em;">Storages
  <ul>
   <li><a href="bweb.pl?action=cmd_storage">Manage Device</a><TMPL_IF achs><hr></TMPL_IF></li>
<TMPL_LOOP achs>
   <li><a href="bweb.pl?action=ach_view;ach=<TMPL_VAR name>"><TMPL_VAR name></a></li>
</TMPL_LOOP>
  </ul>
 </li>
 <li><a href="bweb.pl?action=graph"> Statistics </a>
  <ul>
    <li><a href="bweb.pl?action=graph"> Statistics </a>
    <li><a href="btime.pl"> Backup Timing </a>
    <li><a href="bweb.pl?action=group_stats"> Groups </a>
    <!-- <li><a href="bperf.pl"> Perfs </a> -->
  </ul>
 </li>
 <li> <a href="bweb.pl?action=view_conf"> Configuration </a> 
<TMPL_IF enable_security>
  <ul> <li> <a href="bweb.pl?action=view_conf"> Configuration </a> 
       <li> <a href="bweb.pl?action=users"> Manage users </a>
  </ul>
</TMPL_IF>
</li>
 <li> <a href="bweb.pl?action=about"> About </a> </li>
 <li style="padding: 0.25em 2em;float: right;">&nbsp;
<TMPL_IF loginname>Logged as <TMPL_VAR loginname></TMPL_IF>
<TMPL_IF cur_name>on <TMPL_VAR cur_name></TMPL_IF>
</li>
 <li style="float: right;white-space: nowrap;">
<button type="submit" class="bp" class="button" title="Search media" onclick="search_media();"><img src="/bweb/tape.png" alt=''></button><button type="submit" title="Search client" onclick="search_client();" class='bp'><img src="/bweb/client.png" alt=''></button><input class='formulaire' style="margin: 0 2px 0 2px; padding: 0 0 0 0;" id='searchbox' type='text' size='8' value='search...' onclick="this.value='';" title="Search media or client"></li>
</ul>

<form name="search" action="bweb.pl?" method='GET'>
 <input type="hidden" name="action" value=''>
 <input type="hidden" name="re_media" value=''>
 <input type="hidden" name="re_client" value=''>
</form>

<div style="clear: left;">
<div style="float: left;">
