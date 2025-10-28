import { Paper, Typography, Box } from '@mui/material';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts';
import { format } from 'date-fns';

const TimeSeriesChart = ({ 
  data = [], 
  title, 
  dataKeys = [], 
  colors = ['#1976d2', '#dc004e', '#ff9800', '#4caf50'],
  height = 300,
  yAxisLabel = '',
}) => {
  const formatXAxis = (timestamp) => {
    try {
      return format(new Date(timestamp), 'HH:mm:ss');
    } catch (err) {
      return timestamp;
    }
  };

  const formatTooltip = (value, name) => {
    if (typeof value === 'number') {
      return value.toFixed(2);
    }
    return value;
  };

  const formatTooltipLabel = (label) => {
    try {
      return format(new Date(label), 'MMM dd, HH:mm:ss');
    } catch (err) {
      return label;
    }
  };

  return (
    <Paper elevation={2} sx={{ p: 2 }}>
      {title && (
        <Typography variant="h6" gutterBottom>
          {title}
        </Typography>
      )}
      
      {data.length === 0 ? (
        <Box 
          sx={{ 
            height, 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'center',
            color: 'text.secondary',
          }}
        >
          <Typography>No data available</Typography>
        </Box>
      ) : (
        <ResponsiveContainer width="100%" height={height}>
          <LineChart data={data} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis 
              dataKey="timestamp" 
              tickFormatter={formatXAxis}
              label={{ value: 'Time', position: 'insideBottom', offset: -5 }}
            />
            <YAxis 
              label={{ value: yAxisLabel, angle: -90, position: 'insideLeft' }}
            />
            <Tooltip 
              formatter={formatTooltip}
              labelFormatter={formatTooltipLabel}
            />
            <Legend />
            {dataKeys.map((key, index) => (
              <Line
                key={key.dataKey || key}
                type="monotone"
                dataKey={key.dataKey || key}
                name={key.name || key}
                stroke={colors[index % colors.length]}
                strokeWidth={2}
                dot={false}
                activeDot={{ r: 6 }}
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      )}
    </Paper>
  );
};

export default TimeSeriesChart;
