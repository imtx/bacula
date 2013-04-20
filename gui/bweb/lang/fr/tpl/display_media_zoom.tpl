<table>
<td valign='top'>
 <div class='titlediv'>
  <h1 class='newstitle'> Volume: <TMPL_VAR volumename> <TMPL_VAR comment></h1>
 </div>
 <div class='bodydiv'>
    <b> Volume Infos</b><br/>
    <table id='id_info_<TMPL_VAR volumename>'></table>
    <b> Volume Stats</b><br/>
    <table id='id_media_<TMPL_VAR volumename>'></table>
    <b> Job List </b></br>
    <table id='id_jobs_<TMPL_VAR volumename>'></table>
    <b> Actions </b></br>
   <form action='?' method='get'>
      <input type='hidden' name='media' value='<TMPL_VAR volumename>'>
<TMPL_IF online>&nbsp;
      <button type="submit" class="bp" name='action' value='extern' onclick='return confirm("Do you want to eject this volume ?");' title='move out'> <img src='/bweb/extern.png' alt=''>Eject</button>
<TMPL_ELSE>
      <button type="submit" class="bp" name='action' value='intern' title='move in'> <img src='/bweb/intern.png' alt=''>Load</button>
</TMPL_IF>
      <button type="submit" class="bp" name='action' value='update_media' title='Update'><img src='/bweb/edit.png' alt=''>Edit</button>
      <button type="submit" class="bp" name='action' value='purge' title='Purge'> <img src='/bweb/purge.png' onclick="return confirm('Do you want to purge this volume?')" alt=''>Purge</button>
      <button type="submit" class="bp" name='action' value='prune' title='Prune'> <img src='/bweb/prune.png' alt=''>Prune</button>
<TMPL_IF Locationlog>
      <a href='#' onclick='document.getElementById("locationlog").style.visibility="visible";'><img title='View location log' src='/bweb/zoom.png'></a>
</TMPL_IF>
   </form>
 </div>
</td>
<td valign='top'style="visibility:hidden;" id='locationlog'>
 <div class='titlediv'>
  <h1 class='newstitle'>Location log </h1>
 </div>
 <div class='bodydiv'>
<pre>
 <TMPL_VAR LocationLog>
</pre>
 </div>
</td>
</table>
<script type="text/javascript" language="JavaScript">

var header = new Array("Pool","Online","Enabled", "Location","Vol Status", "Vol Bytes", "Expire",
	               "Retention","Max use duration", "Max jobs" );

var data = new Array();
var img;

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

data.push( new Array(
"<TMPL_VAR poolname>",
img,
human_enabled("<TMPL_VAR enabled>"),
"<TMPL_VAR location>",
"<TMPL_VAR volstatus>",
human_size(<TMPL_VAR nb_bytes>),
timestamp_to_iso("<TMPL_VAR expire>"),
human_sec(<TMPL_VAR volretention>),
human_sec(<TMPL_VAR voluseduration>),
"<TMPL_VAR maxvoljobs>"
 )
);

nrsTable.setup(
{
 table_name:     "id_info_<TMPL_VAR volumename>",
 table_header: header,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 table_data: data,
 header_color: header_color,
 padding: 3,
 disable_sorting: new Array(1)
}
);

var header = new Array( "Vol Mounts", "Recycle count", "Read time", "Write time", "Errors");

var data = new Array();
data.push( new Array(
"<TMPL_VAR nb_mounts>",
"<TMPL_VAR recyclecount>",
human_sec(<TMPL_VAR volreadtime>),
human_sec(<TMPL_VAR volwritetime>),
"<TMPL_VAR nb_errors>"
 )
);

nrsTable.setup(
{
 table_name:     "id_media_<TMPL_VAR volumename>",
 table_header: header,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 table_data: data,
 header_color: header_color,
// disable_sorting: new Array()
 padding: 3
}
);


var header = new Array("JobId","Name","Start Time","Type",
	               "Level","Files","Bytes","Status");

var data = new Array();
var a;
var img;

<TMPL_LOOP jobs>
a = document.createElement('A');
a.href='?action=job_zoom;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src="/bweb/<TMPL_VAR status>.png";
img.title=jobstatus['<TMPL_VAR status>']; 

a.appendChild(img);

data.push( new Array(
"<TMPL_VAR jobid>",
"<TMPL_VAR name>",
"<TMPL_VAR starttime>",
"<TMPL_VAR type>",
"<TMPL_VAR level>",
"<TMPL_VAR files>",
human_size(<TMPL_VAR bytes>),
a
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_jobs_<TMPL_VAR volumename>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: false,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 padding: 3,
// disable_sorting: new Array(5,6),
 rows_per_page: rows_per_page
}
);
</script>
