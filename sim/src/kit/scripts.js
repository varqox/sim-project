function f(w){w.parentNode.setAttribute('class',(w.parentNode.getAttribute('class')=='dropdown'?'dropdown-open':'dropdown'));}
function updateClock()
{
	if(load<0) time_difference=Date.parse(Date())-(start_time += load=  window.performance.timing.domContentLoadedEventEnd - window.performance.timing.navigationStart);
	var currentTime = new Date(Date.parse(Date())-time_difference);
	var currentHours = currentTime.getHours ( );
	var currentMinutes = currentTime.getMinutes ( );
	var currentSeconds = currentTime.getSeconds ( );
	currentMinutes = ( currentMinutes < 10 ? "0" : "" ) + currentMinutes;
	currentSeconds = ( currentSeconds < 10 ? "0" : "" ) + currentSeconds;
	// Compose the string for display
	var currentTimeString = currentHours + ":" + currentMinutes + ":" + currentSeconds;
	var loadTime = window.performance.timing.domContentLoadedEventEnd - window.performance.timing.navigationStart;
	// Update the time display
	document.getElementById("clock").innerHTML = currentTimeString;
	setTimeout(function(){updateClock()},200);
}