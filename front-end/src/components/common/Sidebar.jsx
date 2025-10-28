import { useState } from 'react';
import {
  Drawer,
  List,
  ListItem,
  ListItemButton,
  ListItemIcon,
  ListItemText,
  Toolbar,
  Divider,
  Collapse,
} from '@mui/material';
import { Link, useLocation } from 'react-router-dom';
import DashboardIcon from '@mui/icons-material/Dashboard';
import SettingsIcon from '@mui/icons-material/Settings';
import SendIcon from '@mui/icons-material/Send';
import SystemUpdateIcon from '@mui/icons-material/SystemUpdate';
import ArticleIcon from '@mui/icons-material/Article';
import BuildIcon from '@mui/icons-material/Build';
import BugReportIcon from '@mui/icons-material/BugReport';
import HistoryIcon from '@mui/icons-material/History';
import QueueIcon from '@mui/icons-material/Queue';
import ExpandLess from '@mui/icons-material/ExpandLess';
import ExpandMore from '@mui/icons-material/ExpandMore';

const drawerWidth = 240;

const menuItems = [
  { text: 'Dashboard', icon: <DashboardIcon />, path: '/' },
  { 
    text: 'Configuration', 
    icon: <SettingsIcon />, 
    path: '/config',
    subItems: [
      { text: 'Configure Device', path: '/config?tab=configure' },
      { text: 'Config History', path: '/config?tab=history' },
    ]
  },
  { 
    text: 'Commands', 
    icon: <SendIcon />, 
    path: '/commands',
    subItems: [
      { text: 'Send Command', path: '/commands?tab=send' },
      { text: 'Command Queue', path: '/commands?tab=queue' },
      { text: 'Command History', path: '/commands?tab=history' },
    ]
  },
  { text: 'FOTA', icon: <SystemUpdateIcon />, path: '/fota' },
  { text: 'Logs', icon: <ArticleIcon />, path: '/logs' },
  { text: 'Utilities', icon: <BuildIcon />, path: '/utilities' },
  { text: 'Testing', icon: <BugReportIcon />, path: '/testing' },
];

const Sidebar = ({ open, onClose }) => {
  const location = useLocation();
  const [expandedItems, setExpandedItems] = useState({});

  const handleExpandClick = (itemText) => {
    setExpandedItems(prev => ({
      ...prev,
      [itemText]: !prev[itemText]
    }));
  };

  return (
    <Drawer
      variant="persistent"
      open={open}
      sx={{
        width: drawerWidth,
        flexShrink: 0,
        '& .MuiDrawer-paper': {
          width: drawerWidth,
          boxSizing: 'border-box',
        },
      }}
    >
      <Toolbar />
      <Divider />
      <List>
        {menuItems.map((item) => (
          <div key={item.text}>
            <ListItem disablePadding>
              <ListItemButton
                component={item.subItems ? 'div' : Link}
                to={item.subItems ? undefined : item.path}
                selected={!item.subItems && location.pathname === item.path}
                onClick={() => {
                  if (item.subItems) {
                    handleExpandClick(item.text);
                  } else {
                    onClose();
                  }
                }}
              >
                <ListItemIcon>{item.icon}</ListItemIcon>
                <ListItemText primary={item.text} />
                {item.subItems && (
                  expandedItems[item.text] ? <ExpandLess /> : <ExpandMore />
                )}
              </ListItemButton>
            </ListItem>
            
            {item.subItems && (
              <Collapse in={expandedItems[item.text]} timeout="auto" unmountOnExit>
                <List component="div" disablePadding>
                  {item.subItems.map((subItem) => (
                    <ListItemButton
                      key={subItem.text}
                      component={Link}
                      to={subItem.path}
                      sx={{ pl: 4 }}
                      onClick={onClose}
                    >
                      <ListItemIcon>
                        {subItem.text.includes('History') ? <HistoryIcon fontSize="small" /> : <QueueIcon fontSize="small" />}
                      </ListItemIcon>
                      <ListItemText primary={subItem.text} />
                    </ListItemButton>
                  ))}
                </List>
              </Collapse>
            )}
          </div>
        ))}
      </List>
    </Drawer>
  );
};

export default Sidebar;
