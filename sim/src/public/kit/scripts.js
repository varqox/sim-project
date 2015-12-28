window.onload = function updateClock()
{
	if (typeof updateClock.time_difference === 'undefined') {
		updateClock.time_difference = window.performance.timing.responseStart - start_time;
		// console.log('diff:   ' + (updateClock.time_difference));
		// console.log('date:   ' + ((new Date()).getTime()));
		// console.log('server: ' + ((new Date()).getTime() - updateClock.time_difference));
		// console.log('start:  ' + (start_time));
		// console.log('load:   ' + ((new Date()).getTime() - window.performance.timing.responseStart));
		// console.log('end:    ' + (window.performance.timing.domContentLoadedEventEnd));
		// console.log('begin:  ' + (window.performance.timing.responseStart));
		// console.log(window.performance);
	}
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
	setTimeout(function(){ updateClock() }, 1000 - time.getMilliseconds());
}
$(document).ready(function(){
	$('.dropdown > .dropdown-toggle').click(function(event){
		event.preventDefault();
		if($(this).parent().is('.open'))
			$(this).parent().removeClass('open');
		else
		{
			$('.dropdown.open').removeClass('open');
			$(this).parent().addClass('open');
		}
	});
	$(document).click(function(event){
		if(!$(event.target).is('.dropdown-toggle, .dropdown-toggle *'))
			$('.dropdown.open').removeClass('open');
	});
});
