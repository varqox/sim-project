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
$stmt->bindValue(1, $_SESSION['id'], PDO::PARAM_STR);
$stmt->execute();
if(!($row = $stmt->fetch()))
	E_403();
$user_privileges = privileges($row[0]);
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges,begin_time,end_time,author FROM rounds WHERE id=?");
$stmt->bindValue(1, $_GET['id'], PDO::PARAM_INT);
$stmt->execute();
if(1 > $stmt->rowCount())
	E_404();
$time = time();
$row = $stmt->fetch();
$path = "<a href=\"/round.php?id=".$_GET['id']."\">$row[0]</a>";
$parent = $row[1];
if(privileges($row[2]) < $user_privileges)
	E_403();
if(isset($row[3]) && $_SESSION['id'] != $row[5] && privileges($row[2]) == $user_privileges)
{
	if($time < strtotime($row[3]))
		E_403();
	if($time > strtotime($row[4]))
		$round_is_closed = true;
}
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges FROM rounds WHERE id=?");
while($parent > 1)
{
	$stmt->bindValue(1, $parent, PDO::PARAM_INT);
	$stmt->execute();
	$row = $stmt->fetch();
	$path = "<a href=\"/round.php?id=$parent\">$row[0]</a> / $path";
	$parent = $row[1];
	if(privileges($row[2]) < $user_privileges)
		E_403();
	$stmt->closeCursor();
}

// Move it somewhere else or write better
/*if(isset($data['task_id']))
{
	header('Content-type: application/pdf');
	header('Content-Disposition: filename="'.substr(file($_SERVER['DOCUMENT_ROOT']."/../tasks/".$data['task_id']."/conf.cfg")[0], 0, -1).'.pdf"');
	readfile($_SERVER['DOCUMENT_ROOT']."/../tasks/".$data['task_id']."/content.pdf");
	exit;
}*/

template_begin('Round');

echo '<div class="round-info">',$path,"</div>";

$stmt = DB::pdo()->prepare("SELECT r.id,t.name FROM rounds r, tasks t WHERE r.parent=? AND r.task_id=t.id");
$stmt->bindValue(1, $_GET['id'], PDO::PARAM_INT);
$stmt->execute();

if($stmt->rowCount())
{
	echo "<table class=\"table\"><thead><tr><th>Task</th><th>Actions</th></tr><thead><tbody>";
	while($row = $stmt->fetch())
	{
		echo "<tr><td><a href=\"?id=",$row[0],"\">",$row[1],"</a></td><td><a href=\"/submit.php?round=",$row[0],"\">Submit Solution</a></td></tr>";
	}
	echo "</tbody></table>\n</div>";
}
else
{
	$stmt->closeCursor();
	$stmt = DB::pdo()->prepare("SELECT id,name,begin_time,end_time,privileges FROM rounds WHERE parent=? AND task_id IS NULL");
	$stmt->bindValue(1, $_GET['id'], PDO::PARAM_INT);
	$stmt->execute();
	$no_round = true;
	while($row = $stmt->fetch())
	{
		if($user_privileges <= privileges($row[4]) && time() >= strtotime($row[2]))
		{
			if($no_round)
			{
				$no_round = false;
				echo "<table class=\"table\"><thead><tr><th>Round</th><th>Begin</th><th>End</th></tr><thead><tbody>";
			}
			echo "<tr><td><a href=\"?id=",$row[0],"\">",$row[1],"</a></td><td>",$row[2],"</td><td>",$row[3],"</td></tr>";
		}
	}
	if($no_round)
		echo "<p>Sorry, there are no problems to solve...</p>";
	else
		echo "</tbody></table>\n</div>";
}
$stmt->closeCursor();
template_end();
?>