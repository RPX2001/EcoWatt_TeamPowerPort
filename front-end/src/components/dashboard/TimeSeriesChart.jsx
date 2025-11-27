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
  height = 400,
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
    <Paper 
      elevation={3} 
      sx={{ 
        p: 3, 
        height: '100%',
        width: '100%',
        borderRadius: 2,
        boxShadow: '0 4px 6px rgba(0,0,0,0.1)'
      }}
    >
      {title && (
        <Typography 
          variant="h6" 
          gutterBottom 
          sx={{ 
            fontWeight: 600, 
            mb: 2,
            color: 'primary.main'
          }}
        >
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
          <LineChart data={data} margin={{ top: 10, right: 50, left: 10, bottom: 30 }}>
            <CartesianGrid strokeDasharray="3 3" stroke="#e0e0e0" opacity={0.5} />
            <XAxis 
              dataKey="timestamp" 
              tickFormatter={formatXAxis}
              label={{ value: 'Time', position: 'insideBottom', offset: -15, style: { fontWeight: 600 } }}
              tick={{ fontSize: 13 }}
              tickMargin={10}
            />
            <YAxis 
              label={{ value: yAxisLabel, angle: -90, position: 'insideLeft', style: { fontWeight: 600 } }}
              tick={{ fontSize: 13 }}
              width={70}
              tickMargin={5}
            />
            <Tooltip 
              formatter={formatTooltip}
              labelFormatter={formatTooltipLabel}
              contentStyle={{ 
                backgroundColor: 'rgba(255, 255, 255, 0.98)', 
                border: '1px solid #ccc',
                borderRadius: '8px',
                padding: '12px',
                boxShadow: '0 2px 8px rgba(0,0,0,0.15)'
              }}
            />
            <Legend 
              wrapperStyle={{ paddingTop: '15px' }}
              iconType="line"
              iconSize={20}
            />
            {dataKeys.map((key, index) => (
              <Line
                key={key.dataKey || key}
                type="monotone"
                dataKey={key.dataKey || key}
                name={key.name || key}
                stroke={colors[index % colors.length]}
                strokeWidth={3}
                dot={false}
                activeDot={{ r: 8, strokeWidth: 2 }}
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      )}
    </Paper>
  );
};

export default TimeSeriesChart;
