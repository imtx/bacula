<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
__Autochanger:__ <TMPL_VAR Name> (<TMPL_VAR nb_drive> __Drives__
<TMPL_IF nb_io><TMPL_VAR nb_io> __IMPORT/EXPORT__</TMPL_IF>)</h1>
 </div>
 <div class='bodydiv'>
   <form action='?' method='get'>
    <input type='hidden' name='ach' value='<TMPL_VAR name>'>
    <TMPL_IF "Update">
    <font color='red'> __You must run update slot, Autochanger status is different from bacula slots__ </font>
    <br/>
    </TMPL_IF>
    <table border='0'>
    <tr>
    <td valign='top'>
    <div class='otherboxtitle'>
     __Tools__
    </div>
    <div class='otherbox'>
<button type="submit" class="bp" name='action' value='label_barcodes'
 title='__run label barcodes__'><img src='/bweb/label.png' alt=''>__Label__</button>
<TMPL_IF nb_io>
<button type="submit" class="bp" name='action' value='eject'
 title='__put selected media on i/o__'><img src='/bweb/extern.png' alt=''>__Eject__</button>
<button type="submit" class="bp" name='action' value='clear_io'
 title='__clear I/O__'> <img src='/bweb/intern.png' alt=''>__Clear I/O__</button>
</TMPL_IF>
<button type="submit" class="bp" name='action' value='update_slots'
 title='__run update slots__'> <img src='/bweb/update.png' alt=''>__Update__</button>
<br/><br/>
<button type="submit" class="bp" name='action' value='ach_load'
 title='__mount drive__'> <img src='/bweb/load.png' alt=''>__Mount__</button>
<button type="submit" class="bp" name='action' value='ach_unload'
 title='__umount drive__'> <img src='/bweb/unload.png' alt=''>__Umount__</button>

   </div>
    <td width='200'/>
    <td>
    <b> __Drives:__ </b><br/>
    <table id='id_drive'></table> <br/>
    </td>
    </tr>
    </table>
    <b> __Content:__ </b><br/>
    <table id='id_ach'></table>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("__Real Slot__", "__Slot__", "__Volume Name__",
		       "__Vol Bytes__","__Vol Status__",
	               "__Media Type__","__Pool Name__","__Last Written__", 
                       "__When expire ?__", "__Select__");

var data = new Array();
var chkbox;

<TMPL_LOOP Slots>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'slot';
chkbox.value = '<TMPL_VAR realslot>';

data.push( new Array(
"<TMPL_VAR realslot>",
"<TMPL_VAR slot>",
"<TMPL_VAR volumename>",
human_size(<TMPL_VAR volbytes>),
"<TMPL_VAR volstatus>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR name>",
"<TMPL_VAR lastwritten>",
timestamp_to_iso("<TMPL_VAR expire>"),
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_ach",
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
// page_nav: true,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
 padding: 3
}
);

var header = new Array("__Index__", "__Drive Name__", 
		       "__Volume Name__", "__Select__");

var data = new Array();
var chkbox;

<TMPL_LOOP Drives>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'drive';
chkbox.value = '<TMPL_VAR index>';

data.push( new Array(
"<TMPL_VAR index>",
"<TMPL_VAR name>",
"<TMPL_VAR load>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_drive",
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
// page_nav: true,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6),
 padding: 3
}
);

</script>
