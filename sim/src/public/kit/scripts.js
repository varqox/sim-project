// function f(w){w.parentNode.setAttribute('class',(w.parentNode.getAttribute('class')=='dropdown'?'dropdown-open':'dropdown'));}
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

	var currentTime = new Date((new Date()).getTime() - updateClock.time_difference);
	var currentHours = currentTime.getUTCHours();
	var currentMinutes = currentTime.getUTCMinutes();
	var currentSeconds = currentTime.getUTCSeconds();
	currentHours = (currentHours < 10 ? "0" : "") + currentHours;
	currentMinutes = (currentMinutes < 10 ? "0" : "") + currentMinutes;
	currentSeconds = (currentSeconds < 10 ? "0" : "") + currentSeconds;
	// Compose the string for display
	var currentTimeString = currentHours + ":" + currentMinutes + ":" + currentSeconds + " UTC";
	// Update the displayed time
	document.getElementById("clock").innerHTML = currentTimeString;
	setTimeout(function(){ updateClock() }, 200);
}
$(document).ready(function(){
	$('.dropdown > a').click(function(event){
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
		if(!$(event.target).is('.open > a.user, .open > a.user *'))
			$('.dropdown.open').removeClass('open');
	});
});
