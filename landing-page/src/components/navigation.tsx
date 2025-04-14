import React, { useEffect, useState } from 'react';
import "../styles/global.css";

interface MenuItemsProps {
  id: string;
  label: string;
  selected: boolean;
  onClick?: (e: { id: string }) => void;
}

interface MenuProps {
  items: MenuItemsProps[];
  onClick: (e: { id: string }) => void;
  selectedKeys: string[];  // Changed to an array of strings
  mode: string;
}

// MenuItem component uses the "id" instead of "key".
function MenuItem(props: MenuItemsProps) {
  return (
      <a  
      className={`menu-item ${props.selected ? 'menu-item-selected' : ''}`}
      onClick={() => {
        // If the MenuItem has its own click handler, call it.
        if (props.onClick) {
          props.onClick({ id: props.id });
        }
      }} 
      href={props.id === 'home' ? '/' : `/${props.id}`}
      >{props.label}</a>
  );
}

// Menu component maps over its items and passes along the correct id.
function Menu(props: MenuProps) {
  console.log('Menu received selectedKeys:', props.selectedKeys);
  
  return (
    <nav className={`menu menu-${props.mode}`}>
      <ul className={`menu-ul-${props.mode}`}>
        {props.items.map(item => (
          // Use item.id as the key for React here
          <MenuItem
            key={item.id}
            id={item.id}
            label={item.label}
            onClick={() => {
              // Call the onClick passed to the Menu component.
              props.onClick({ id: item.id });
              
            }}
            selected={props.selectedKeys.includes(item.id)}
          />
        ))
        
        }
      </ul>
    </nav>
  );
}

// Define your menu items using "id" instead of "key".
const MenuItems: MenuItemsProps[] = [
  { id: 'home', label: 'Home', onClick: (e) => console.log('Clicked:', e.id), selected: false },
  { id: 'device', label: 'Devices', onClick: (e) => console.log('Clicked:', e.id), selected: false  },
  { id: 'mobileapp', label: 'Mobile App', onClick: (e) => console.log('Clicked:', e.id), selected: false  },
  { id: 'about', label: 'About us', onClick: (e) => console.log('Clicked:', e.id), selected: false  },
];

const Navigation: React.FC = () => {
  const [current, setCurrent] = useState<string>('');
  const [isMobile, setIsMobile] = useState<boolean>(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState<boolean>(false);


  // Listen to window resize events to toggle between mobile & desktop.
  const handleResize = () => {
    setIsMobile(window.innerWidth < 768);
  };
  useEffect(() => {
    const pathSegments = window.location.pathname.split('/');
    // The first element is empty (because pathnames start with a slash)
    const currentPath = pathSegments[1] || 'home';
    setCurrent(currentPath);
    console.log('Setting current path to:', currentPath);

    handleResize(); // initial check
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  const onClick = (e: { id: string }) => {
    setCurrent(e.id);
    if (isMobile) {
      setMobileMenuOpen(false); // Close the mobile menu after a selection.
    }
  };
  // Create a new array with updated 'selected' property based on current state
  const updatedMenuItems = MenuItems.map(item => ({
    ...item,
    selected: item.id === current
  }));

  return (
    <div className="navigation-container">
      {isMobile && (
        <div className="mobile-nav">
          <div className="mobile-header">
            <div className="logo caret-transparent">
              <a href="/">Logo</a>
            </div>
            <div className="hamburger caret-transparent" onClick={() => setMobileMenuOpen(!mobileMenuOpen)}>
              {mobileMenuOpen ? '✕' : '☰'}
            </div>
          </div>
          {/* Mobile menu with slide-in animation  */}
          <div className={`mobile-overlay ${mobileMenuOpen ? 'mobile-overlay-visible' : ''}`} 
               onClick={() => setMobileMenuOpen(false)}></div>
          <div className={`menu-vertical ${!mobileMenuOpen ? 'mobile-menu-closed' : ''}`}>
            <Menu 
              items={updatedMenuItems} 
              onClick={onClick} 
              selectedKeys={[current]} 
              mode="vertical" 
            />
          </div>
        </div>
      )}
      { !isMobile && (
        <div style={{ borderBottom: '1px solid #f0f0f0'}} className='desktop-nav w-full flex flex-row grow text-center justify-center align-middle items-center'>
          <a className="w-140 h-full px-2 no-underline outline-none" href="/">Logo</a>
          <Menu items={updatedMenuItems} onClick={onClick} selectedKeys={[current]} mode="horizontal" />
        </div>
      )}
    </div>
  );
};

export default Navigation;
