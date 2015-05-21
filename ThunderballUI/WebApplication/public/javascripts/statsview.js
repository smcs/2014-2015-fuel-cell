// Connect to the nodeJs Server
var socket = io();
socket.emit('getstatsdata', { some: 'data' });

var chartdata = [],
labels = [];
var chart2data = [],
labels2 = [];

//for (var i = 0; i < 5; i++) {
//    chartdata.push(5 - i);
//    labels.push('Foo' + ' - ' + (i + 1));
//}

var chart;
socket.on('some-data', function (data) {
    for (var i = 0; i < data.length; i++) {
        labels.push(data[i]["Time"]);
		labels2.push(data[i]["Time"]);
        chartdata.push(data[i]["Energy"]);
		chart2data.push(data[i]["FuelFlow"]);
    }
});

socket.on('data-done', function (data) {

    console.log("Query Done");

    // Attach a handler to the window load event.
    chart = new Highcharts.Chart({
        chart: {
            renderTo: 'chart',
            type: 'column'
        },

        title: {
            text: 'Fuel Cell - Net Energy Production'
        },

        xAxis: {
            categories: labels, //Once the Data is fixed these are the x axis titles
            title: {
                text: 'Time (hours)'
            },
            //labels: {
            //    style: {
            //       fontSize: '1.3em',
            //        fontWeight: 'bold'
            //    }
            //}
        },

        yAxis: {
            min: -10,
            title: {
                text: 'Watt-Hours',
                align: 'high'
            },
        },


        plotOptions: {
            bar: {
                dataLabels: {
                    enabled: true,
                    style: {
                        fontSize: '1.3em',
                        fontWeight: 'bold'
                    }
                }
            }
        },

        legend: {
            enabled: false
        },

        credits: {
            enabled: false
        },

        series: [{ name: 'Watt-Hours', data: chartdata }]

    });
	// Attach a handler to the window load event.
    chart2 = new Highcharts.Chart({
        chart: {
            renderTo: 'chart2',
            type: 'column'
        },

        title: {
            text: 'Fuel Cell - Fuel Consumption'
        },

        xAxis: {
            categories: labels, //Once the Data is fixed these are the x axis titles
            title: {
                text: 'Time (hours)'
            },
            //labels: {
            //    style: {
            //       fontSize: '1.3em',
            //        fontWeight: 'bold'
            //    }
            //}
        },

        yAxis: {
            min: 0,
            title: {
                text: 'Pounds',
                align: 'high'
            },
        },

        plotOptions: {
            bar: {
                dataLabels: {
                    enabled: true,
                    style: {
                        fontSize: '1.3em',
                        fontWeight: 'bold'
                    }
                }
            }
        },

        legend: {
            enabled: false
        },

        credits: {
            enabled: false
        },

        series: [{ name: 'Watt-Hours', data: chart2data }]

    });
});


