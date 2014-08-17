<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if(!isset($_GET['id']) || !is_numeric($_GET['id']))
	$_GET['id']=1;

if(!check_logged_in())
{
	header("Location: /login.php");
	exit;
}

$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE id=?");
$stmt->bindValue(1, $_SESSION['id']);
$stmt->execute();
if(!($row = $stmt->fetch()))
	E_403();
$user_privileges = privileges($row[0]);
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges,begin_time,end_time,author,task_id FROM rounds WHERE id=?");
$stmt->bindValue(1, $_GET['id']);
$stmt->execute();
if(1 > $stmt->rowCount())
	E_404();
$time = time();
$row = $stmt->fetch();
$name = $row[0];
$path = "<a href=\"/round.php?id=".$_GET['id']."\">$name</a>";
$parent = $row[1];
$author = $row[5];
$task_id = $row[6];
$begin_time = $row[3];
$end_time = $row[4];
$round_privileges = privileges($row[2]);
if($round_privileges < $user_privileges)
	E_403();
if(isset($row[3]) && $_SESSION['id'] != $row[5] && $round_privileges == $user_privileges && $time < strtotime($begin_time))
		E_403();
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges FROM rounds WHERE id=?");
while($parent > 1)
{
	$stmt->bindValue(1, $parent);
	$stmt->execute();
	$row = $stmt->fetch();
	$path = "<a href=\"/round.php?id=$parent\">$row[0]</a> / $path";
	$parent = $row[1];
	if(privileges($row[2]) < $user_privileges)
		E_403();
	$stmt->closeCursor();
}

if(isset($task_id))
{
	if(isset($_GET['content']))
	{
		if(file_exists($_SERVER['DOCUMENT_ROOT']."/../tasks/".$task_id."/content.pdf"))
		{
			header('Content-type: application/pdf');
			header('Content-Disposition: filename="'.substr(file($_SERVER['DOCUMENT_ROOT']."/../tasks/".$task_id."/conf.cfg")[0], 0, -1).'.pdf"');
			readfile($_SERVER['DOCUMENT_ROOT']."/../tasks/".$task_id."/content.pdf");
			exit;
		}
		E_404();
	}
	else
	{
		// Need to write good task view
		template_begin('Round','','.body{margin-left:150px}');
		echo '<ul class="menu"><li><a href="submissions/?id=',$_GET['id'],'">Submissions</a></li></ul>';
		echo '<div class="path">',$path,"</div><div class=\"round-info\"><p style=\"font-size:30px;margin:10px 0\">$name</p><p>Beginning: ",(isset($begin_time) ? $begin_time : "whenever"),"</p><p>End: ",(isset($end_time) ? $end_time : "never"),'</p></div><a class="btn-small" href="?id=',$_GET['id'],'&content">View content</a>',($round_privileges == $user_privileges && $_SESSION['id'] != $author && isset($end_time) && $time > $end_time ? "" : '<a class="btn-small" href="submit.php?round='.$_GET['id'].'">Submit solution</a>');
		template_end();
		exit;
	}
}

template_begin('Round','','.body{margin-left:150px}');
echo '<ul class="menu"><li><a href="submissions/?id=',$_GET['id'],'">Submissions</a></li></ul>';

if($_GET['id'] != 1)
	echo '<div class="path">',$path,"</div><div class=\"round-info\"><p style=\"font-size:30px;margin:10px 0\">$name</p><p>Beginning: ",(isset($begin_time) ? $begin_time : "whenever"),"</p><p>End: ",(isset($end_time) ? $end_time : "never"),"</p></div>";

// Here we will take care on printing subrounds...
// First we need to select all subrounds (problems too)
$stmt = DB::pdo()->prepare("SELECT id,item,visible,author,name,begin_time,end_time,privileges,task_id FROM rounds WHERE parent=?");
$stmt->bindValue(1, $_GET['id']);
$stmt->execute();

// Here we will select all rounds to print
$no_round = true;
$col_rounds = array();
while($row = $stmt->fetch())
{
	// If user have access
	if($_SESSION['id'] == $row[3] || privileges($row[7]) > $user_privileges || (privileges($row[7]) == $user_privileges && (!isset($row[5]) || strtotime($row[5]) <= $time || $row[2])))
	{
		if($no_round)
			$no_round = false;
		// Collect subround
		if(!isset($col_rounds[$row[1]]))
			$col_rounds[$row[1]] = array();
		if(isset($row[8]))
			$col_rounds[$row[1]][] = "<tr><td><a href=\"?id=$row[0]&content\">$row[4]</a></td><td>".(isset($row[5]) ? $row[5] : "whenever")."</td><td>".(isset($row[6]) ? $row[6] : "never")."</td><td>".(privileges($row[7]) == $user_privileges && $_SESSION['id'] != $row[3] && isset($row[6]) && $time > strtotime($row[6]) ? "" : "<a class=\"btn-small\" href=\"submit.php?round=$row[0]\">Submit solution</a>")."<a class=\"btn-small\" href=\"?id=$row[0]\">View as round</a></td></tr>";
		else
			$col_rounds[$row[1]][] = "<tr><td><a href=\"?id=$row[0]\">$row[4]</a></td><td>".(isset($row[5]) ? $row[5] : "whenever")."</td><td>".(isset($row[6]) ? $row[6] : "never")."</td><td></td></tr>";

	}
}
$stmt->closeCursor();

// Sorting collected data
ksort($col_rounds);

// And print
if($no_round)
	echo "<p>Sorry, there are no problems to solve...</p>";
else
{
	// Here right printing
	echo "<table class=\"table rounds\"><thead><tr><th>Name</th><th>Beginning</th><th>End</th><th>Actions</th></tr></thead><tbody>";
	foreach ($col_rounds as $row)
		foreach ($row as $out)
			echo $out;
	echo "</tbody></table>";
}

template_end();
?>