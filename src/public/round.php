<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if(!isset($_GET['id']) || !is_numeric($_GET['id']))
	E_404();

if(!check_logged_in())
	header("Location: /login.php");

$data = array();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges,task_id FROM rounds WHERE id=?");
$stmt->bindValue(1, $_GET['id'], PDO::PARAM_INT);
$stmt->execute();
if(1 > $stmt->rowCount())
	E_404();
$row = $stmt->fetch();
if(isset($row[3]))
	$data['task_id'] = $row[3];
$data['parent'] = $row[1];
$data['privileges'] = privileges($row[2]);
$path = $row[0];
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges FROM rounds WHERE id=?");
while($data['parent'] > 1)
{
	$stmt->bindValue(1, $data['parent'], PDO::PARAM_INT);
	$stmt->execute();
	$row = $stmt->fetch();
	$path = "<a href=\"?id=".$data['parent']."\">".$row[0]."</a>/".$path;
	$data['parent'] = $row[1];
	$data['privileges'] = min($data['privileges'], privileges($row[2]));
	$stmt->closeCursor();
}

$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE username=?");
$stmt->bindValue(1, $_SESSION['username'], PDO::PARAM_STR);
$stmt->execute();
$user_privilages = 3;
if(!($row = $stmt->fetch()) || ($user_privileges = privileges($row[0])) > $data['privileges'])
	E_403();
$stmt->closeCursor();

if(isset($data['task_id']))
{
	header('Content-type: application/pdf');
	header('Content-Disposition: filename="'.substr(file($_SERVER['DOCUMENT_ROOT']."/../tasks/".$data['task_id']."/conf.cfg")[0], 0, -1).'.pdf"');
	readfile($_SERVER['DOCUMENT_ROOT']."/../tasks/".$data['task_id']."/content.pdf");
	exit;
}

template_begin('Contests');

echo "<pre>";print_r($data);echo "</pre>";
echo $path;

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
	echo "</tbody></table>\n";
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
		if($user_privileges <= privileges($row[4]))
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
		echo "</tbody></table>\n";
}
$stmt->closeCursor();
template_end();
?>