/**
 * FOTA (Firmware Over-The-Air) Management Page
 * 
 * Main page for firmware management with three tabs:
 * 1. Upload - Upload new firmware files
 * 2. Manage - View and manage firmware versions
 * 3. Progress - Monitor OTA update progress
 */

import React, { useState, useEffect } from 'react';
import {
  Box,
  Container,
  Typography,
  Tabs,
  Tab,
  Paper
} from '@mui/material';
import {
  CloudUpload as UploadIcon,
  Storage as ManageIcon,
  TrendingUp as ProgressIcon
} from '@mui/icons-material';
import { useSearchParams } from 'react-router-dom';
import FirmwareUpload from '../components/fota/FirmwareUpload';
import FirmwareList from '../components/fota/FirmwareList';
import OTAProgress from '../components/fota/OTAProgress';

const FOTA = () => {
  const [searchParams, setSearchParams] = useSearchParams();
  const [activeTab, setActiveTab] = useState(0);

  // Map tab names to indices
  const tabMap = {
    upload: 0,
    manage: 1,
    progress: 2
  };

  // Initialize tab from URL parameter
  useEffect(() => {
    const tab = searchParams.get('tab');
    if (tab && tabMap[tab] !== undefined) {
      setActiveTab(tabMap[tab]);
    }
  }, [searchParams]);

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
    
    // Update URL parameter
    const tabName = Object.keys(tabMap).find(key => tabMap[key] === newValue);
    if (tabName) {
      setSearchParams({ tab: tabName });
    }
  };

  return (
    <Container maxWidth="xl" sx={{ py: 4 }}>
      {/* Page Header */}
      <Box sx={{ mb: 4 }}>
        <Typography variant="h4" component="h1" gutterBottom>
          Firmware Over-The-Air (FOTA) Management
        </Typography>
        <Typography variant="body1" color="text.secondary">
          Upload, manage, and deploy firmware updates to your devices
        </Typography>
      </Box>

      {/* Tabs */}
      <Paper sx={{ mb: 3 }}>
        <Tabs 
          value={activeTab} 
          onChange={handleTabChange}
          variant="fullWidth"
          sx={{ borderBottom: 1, borderColor: 'divider' }}
        >
          <Tab 
            icon={<UploadIcon />} 
            label="Upload Firmware"
            iconPosition="start"
          />
          <Tab 
            icon={<ManageIcon />} 
            label="Manage Firmware"
            iconPosition="start"
          />
          <Tab 
            icon={<ProgressIcon />} 
            label="Update Progress"
            iconPosition="start"
          />
        </Tabs>
      </Paper>

      {/* Tab Content */}
      <Box>
        {activeTab === 0 && <FirmwareUpload />}
        {activeTab === 1 && <FirmwareList />}
        {activeTab === 2 && <OTAProgress />}
      </Box>
    </Container>
  );
};

export default FOTA;
