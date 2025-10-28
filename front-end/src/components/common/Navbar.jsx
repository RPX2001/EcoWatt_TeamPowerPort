import { AppBar, Toolbar, Typography, Box, IconButton } from '@mui/material';
import MenuIcon from '@mui/icons-material/Menu';
import PowerSettingsNewIcon from '@mui/icons-material/PowerSettingsNew';

const Navbar = ({ onMenuClick }) => {
  return (
    <AppBar position="fixed" sx={{ zIndex: (theme) => theme.zIndex.drawer + 1 }}>
      <Toolbar>
        <IconButton
          color="inherit"
          edge="start"
          onClick={onMenuClick}
          sx={{ mr: 2 }}
        >
          <MenuIcon />
        </IconButton>
        
        <PowerSettingsNewIcon sx={{ mr: 1 }} />
        
        <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
          EcoWatt Control Panel
        </Typography>

        <Typography variant="body2" sx={{ mr: 2 }}>
          v1.0.0
        </Typography>
      </Toolbar>
    </AppBar>
  );
};

export default Navbar;
