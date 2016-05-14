window.onload = function updateClock() {
	if (typeof updateClock.time_difference === 'undefined')
		updateClock.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - updateClock.time_difference);
	var hours = time.getUTCHours();
	var minutes = time.getUTCMinutes();
	var seconds = time.getUTCSeconds();
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;
	// Update the displayed time
	document.getElementById('clock').innerHTML = String().concat(hours, ':', minutes, ':', seconds, ' UTC');
	setTimeout(updateClock, 1000 - time.getMilliseconds());
}
$(document).ready(function(){
	$('.dropmenu > .dropmenu-toggle').click(function(event) {
		event.preventDefault();
		if($(this).parent().is('.open'))
			$(this).parent().removeClass('open');
		else {
			$('.dropmenu.open').removeClass('open');
			$(this).parent().addClass('open');
		}
	});
	$(document).click(function(event) {
		if(!$(event.target).is('.dropmenu-toggle, .dropmenu-toggle *'))
			$('.dropmenu.open').removeClass('open');
	});
});
