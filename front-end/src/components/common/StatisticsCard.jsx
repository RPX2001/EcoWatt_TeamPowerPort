/**
 * StatisticsCard Component
 * 
 * Reusable card component for displaying key metrics
 * Features:
 * - Icon display
 * - Title and value
 * - Percentage change indicator
 * - Color-coded trend
 * - Optional subtitle
 */

import React from 'react';
import {
  Card,
  CardContent,
  Typography,
  Box,
  Avatar
} from '@mui/material';
import {
  TrendingUp as TrendingUpIcon,
  TrendingDown as TrendingDownIcon
} from '@mui/icons-material';

const StatisticsCard = ({
  title,
  value,
  icon: Icon,
  color = 'primary',
  subtitle,
  trend,
  trendValue,
  unit = ''
}) => {
  const getTrendColor = () => {
    if (trend === 'up') return 'success.main';
    if (trend === 'down') return 'error.main';
    return 'text.secondary';
  };

  const getTrendIcon = () => {
    if (trend === 'up') return <TrendingUpIcon fontSize="small" />;
    if (trend === 'down') return <TrendingDownIcon fontSize="small" />;
    return null;
  };

  return (
    <Card sx={{ height: '100%' }}>
      <CardContent>
        <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
          <Avatar
            sx={{
              bgcolor: `${color}.light`,
              color: `${color}.main`,
              width: 56,
              height: 56,
              mr: 2
            }}
          >
            {Icon && <Icon sx={{ fontSize: 30 }} />}
          </Avatar>
          <Box sx={{ flexGrow: 1 }}>
            <Typography variant="body2" color="text.secondary" gutterBottom>
              {title}
            </Typography>
            <Typography variant="h4" component="div" fontWeight="bold">
              {value}{unit}
            </Typography>
          </Box>
        </Box>

        {subtitle && (
          <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
            {subtitle}
          </Typography>
        )}

        {trend && trendValue !== undefined && (
          <Box
            sx={{
              display: 'flex',
              alignItems: 'center',
              color: getTrendColor()
            }}
          >
            {getTrendIcon()}
            <Typography variant="body2" sx={{ ml: 0.5 }}>
              {trendValue}%
            </Typography>
            <Typography variant="body2" color="text.secondary" sx={{ ml: 1 }}>
              vs last period
            </Typography>
          </Box>
        )}
      </CardContent>
    </Card>
  );
};

export default StatisticsCard;
