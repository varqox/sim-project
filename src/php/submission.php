<?php
require_once "main.php";

function template($sid, $initial_tests = NULL, $final_tests = NULL)
{
	if(!check_logged_in())
	{
		header("Location: /login.php");
		exit;
	}

	// Get all data from database

	// User privileges
	$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE id=?");
	$stmt->bindValue(1, $_SESSION['id']);
	$stmt->execute();
	if(!($row = $stmt->fetch()))
		E_403();
	$user_privileges = privileges($row[0]);
	$stmt->closeCursor();

	// Submission
	$stmt = DB::pdo()->prepare("SELECT user_id,round_id,task_id,time,status,points FROM submissions WHERE id=?");
	$stmt->bindValue(1, $sid);
	$stmt->execute();
	if(!($submission = $stmt->fetch()))
		E_403();
	$stmt->closeCursor();

	// Round
	$stmt = DB::pdo()->prepare("SELECT parent,privileges,begin_time,end_time,author,full_judge_time FROM rounds WHERE id=?");
	$stmt->bindValue(1, $submission[1]);
	$stmt->execute();
	if(1 > $stmt->rowCount())
		E_404();
	$time = time();
	$row = $stmt->fetch();
	$parent = $row[0];
	$begin_time = $row[2];
	$end_time = $row[3];
	$full_judge_time = $row[5];
	$round_privileges = privileges($row[1]);
	if($round_privileges < $user_privileges)
		E_403();
	$superuser_access = false;
	if($round_privileges > $user_privileges || $row[4] == $_SESSION['id'])
		$superuser_access = true;
	if(isset($row[2]) && $_SESSION['id'] != $row[4] && $round_privileges == $user_privileges && time() < strtotime($row[2]))
		E_403();
	$stmt->closeCursor();

	// Path
	$stmt = DB::pdo()->prepare("SELECT parent,privileges FROM rounds WHERE id=?");
	while($parent > 1)
	{
		$stmt->bindValue(1, $parent);
		$stmt->execute();
		$row = $stmt->fetch();
		$parent = $row[0];
		if(privileges($row[1]) < $user_privileges)
			E_403();
		$stmt->closeCursor();
	}

	if(!$superuser_access && $_SESSION['id'] != $submission[0])
		E_403();

	// User have access

	if(isset($_GET['download']))
	{
		header('Content-type: application/text');header('Content-Disposition: attchment; filename="'.$sid.'.cpp"');
		readfile($_SERVER['DOCUMENT_ROOT']."/../solutions/".$sid.".cpp");
		exit;
	}

	if(isset($_GET['source']))
	{
		template_begin('Submission '.$sid);
		echo '<div style="margin: 60px 0 0 10px">', shell_exec($_SERVER['DOCUMENT_ROOT']."/../judge/CTH ".$_SERVER['DOCUMENT_ROOT']."/../solutions/".$sid.".cpp"), '</div>';template_end();
		exit;
	}

	template_begin('Submission '.$sid,'','.body{margin-left:150px}');

	echo '<ul class="menu"><li><a href="/round.php?id=',$submission[1],'">View round</a></li><li><a href="/submissions/?id=',$submission[1],'">Submissions</a></li><li><a href="/round.php?id=',$submission[1],'&content">Task content</a></li>',(!$superuser_access && ((isset($begin_time) && strtotime($begin_time) > $time) || (isset($end_time) && $time > strtotime($end_time))) ? "" : '<li><a href="/submit.php?round='.$submission[1].'">Submit a solution</a></li>'),'</ul>';

	$stmt = DB::pdo()->prepare("SELECT name FROM tasks WHERE id=?");
	$stmt->bindValue(1, $submission[2]);
	$stmt->execute();
	$task_name = "";
	if($row = $stmt->fetch())
		$task_name = $row[0];
	$stmt->closeCursor();

	echo '<div style="text-align: center">
<div class="submission-info">
<h1>Zgłoszenie ',$sid,'</h1>
<div class="btn-toolbar">
<a class="btn-small" href="?source">View source</a>
<a class="btn-small" href="?download">Download</a>
</div>
<table style="width:100%">
<thead style="padding-bottom:5px">
<tr>
<th style="min-width:120px">Problem ',$submission[2],'</th>
<th style="min-width:150px">Submission time</th>
<th style="min-width:150px">Status</th>
<th style="min-width:90px">Score</th>
</tr>
</thead>
<tbody>
<tr>
<td>',$task_name,'</td>
<td>',$submission[3],'</td>
<td';
	$show_final = $superuser_access || strtotime($full_judge_time) <= $time;
	switch($submission[4])
	{
		case "ok": echo ' class="ok">Initial tests: OK';break;
		case "error": echo ' class="wa">Initial tests: Error';break;
		case "c_error": echo ' class="wa">Compilation failed';break;
		case "waiting": echo '>Pending';break;
	}
	echo '</td><td>',($show_final ? $submission[5] : ''),'</td></tr></tbody></table></div>';

	// Echo final_tests
	if($show_final && isset($final_tests))
		echo '<h3 style="font-size:20px;font-weight:normal">Final testing report</h3><table style="margin-top: -15px" class="table results">
<thead>
<tr>
<th style="min-width: 80px">Test</th>
<th style="min-width: 190px">Result</th>
<th style="min-width: 100px">Time</th>
<th style="min-width: 70px">Result</th>
</tr>
</thead>
<tbody>',$final_tests,'</tbody></table>';

	// Echo initial_tests
	if(isset($initial_tests))
		echo '<h3 style="font-size:20px;font-weight:normal">Initial testing report</h3><table style="margin-top: -15px" class="table results">
<thead>
<tr>
<th style="min-width: 80px">Test</th>
<th style="min-width: 190px">Result</th>
<th style="min-width: 100px">Time</th>
<th style="min-width: 70px">Result</th>
</tr>
</thead>
<tbody>',$initial_tests,'</tbody></table></div>';

	template_end();
}
?>