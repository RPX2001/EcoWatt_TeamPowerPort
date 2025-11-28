import { Card, CardContent, Typography, Box, Chip } from '@mui/material';
import TrendingUpIcon from '@mui/icons-material/TrendingUp';
import TrendingDownIcon from '@mui/icons-material/TrendingDown';
import TrendingFlatIcon from '@mui/icons-material/TrendingFlat';

const MetricsCard = ({ title, value, unit, icon: Icon, trend, color = 'primary' }) => {
  const getTrendIcon = () => {
    if (!trend) return null;
    
    if (trend > 0) {
      return <TrendingUpIcon fontSize="small" color="success" />;
    } else if (trend < 0) {
      return <TrendingDownIcon fontSize="small" color="error" />;
    } else {
      return <TrendingFlatIcon fontSize="small" color="disabled" />;
    }
  };

  const getTrendColor = () => {
    if (trend > 0) return 'success';
    if (trend < 0) return 'error';
    return 'default';
  };

  return (
    <Card 
      elevation={2} 
      sx={{ 
        height: '100%',
        transition: 'transform 0.2s, box-shadow 0.2s',
        '&:hover': {
          transform: 'translateY(-4px)',
          boxShadow: 4,
        },
      }}
    >
      <CardContent>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2 }}>
          <Typography variant="subtitle2" color="text.secondary" sx={{ fontWeight: 500 }}>
            {title}
          </Typography>
          {Icon && (
            <Box
              sx={{
                bgcolor: `${color}.light`,
                borderRadius: '50%',
                p: 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
              }}
            >
              <Icon sx={{ color: `${color}.main`, fontSize: 24 }} />
            </Box>
          )}
        </Box>

        <Box sx={{ display: 'flex', alignItems: 'baseline', gap: 1 }}>
          <Typography variant="h4" component="div" sx={{ fontWeight: 600, color: `${color}.main` }}>
            {value !== null && value !== undefined ? value : '--'}
          </Typography>
          {unit && (
            <Typography variant="body2" color="text.secondary">
              {unit}
            </Typography>
          )}
        </Box>

        {trend !== null && trend !== undefined && (
          <Box sx={{ mt: 1, display: 'flex', alignItems: 'center', gap: 0.5 }}>
            <Chip
              size="small"
              icon={getTrendIcon()}
              label={`${trend > 0 ? '+' : ''}${trend.toFixed(1)}%`}
              color={getTrendColor()}
              variant="outlined"
              sx={{ height: 24 }}
            />
          </Box>
        )}
      </CardContent>
    </Card>
  );
};

export default MetricsCard;
