import React from 'react';

const Footer: React.FC = () => {
  const currentYear = new Date().getFullYear();
  
  return (
    <footer className="bg-gray-800 text-white py-8">
      <div className="container mx-auto px-4">
        {/* Main Footer Content */}
        <div className="flex flex-col md:flex-row justify-between">
          
          {/* Logo and Description */}
          <div className="mb-6 md:mb-0 md:w-1/3">
            <h2 className="text-xl font-bold mb-4">S.A.G.E</h2>
            <p className="text-gray-300 mb-4">
              Soil Analysis & Ground Evaluation - Autonomous Soil Health Management System
            </p>
          </div>
          
          {/* Quick Links Navigation */}
          <div className="mb-6 md:mb-0 md:w-1/4">
            <h3 className="text-lg font-semibold mb-4">Quick Links</h3>
            <nav>
              <ul className="space-y-2">
                <li><a href="/" className="text-gray-300 hover:text-green-400">Home</a></li>
                <li><a href="/about" className="text-gray-300 hover:text-green-400">Team</a></li>
                <li><a href="/mobileapp" className="text-gray-300 hover:text-green-400">Mobile Application</a></li>
                <li><a href="/device" className="text-gray-300 hover:text-green-400">Device</a></li>
              </ul>
            </nav>
          </div>
          
          {/* Contact Information */}
          <div className="md:w-1/4">
            <h3 className="text-lg font-semibold mb-4">Contact Us</h3>
            <address className="not-italic text-gray-300 mb-4">
              <p>University of Puerto Rico Mayaguez</p>
              <p>Mayaguez, PR</p>
              <p className="mt-2 flex flex-col">
                <a href="mailto:eithan.capella@upr.edu" className="hover:text-green-400">
                    eithan.capella@upr.edu
                </a>
                <a href="mailto:bryan.vega7@upr.edu" className="hover:text-green-400">
                    bryan.vega7@upr.edu
                </a>
                <a href="mailto:emmanuel.gonzalez22@upr.edu" className="hover:text-green-400">
                    emmanuel.gonzalez22@upr.edu
                </a>
              </p>
            </address>
            
            {/* Social Media Links*/}

            <div className="flex space-x-4 mt-4">
              <a href="https://github.com/emmagonz22/S.A.G.E" className="text-gray-300 hover:text-green-400" aria-label="GitHub">
                <svg className="h-6 w-6" fill="currentColor" viewBox="0 0 24 24">
                  <path fillRule="evenodd" d="M12 2C6.477 2 2 6.484 2 12.017c0 4.425 2.865 8.18 6.839 9.504.5.092.682-.217.682-.483 0-.237-.008-.868-.013-1.703-2.782.605-3.369-1.343-3.369-1.343-.454-1.158-1.11-1.466-1.11-1.466-.908-.62.069-.608.069-.608 1.003.07 1.531 1.032 1.531 1.032.892 1.53 2.341 1.088 2.91.832.092-.647.35-1.088.636-1.338-2.22-.253-4.555-1.113-4.555-4.951 0-1.093.39-1.988 1.029-2.688-.103-.253-.446-1.272.098-2.65 0 0 .84-.27 2.75 1.026A9.564 9.564 0 0112 6.844c.85.004 1.705.115 2.504.337 1.909-1.296 2.747-1.027 2.747-1.027.546 1.379.202 2.398.1 2.651.64.7 1.028 1.595 1.028 2.688 0 3.848-2.339 4.695-4.566 4.943.359.309.678.92.678 1.855 0 1.338-.012 2.419-.012 2.747 0 .268.18.58.688.482A10.019 10.019 0 0022 12.017C22 6.484 17.522 2 12 2z" clipRule="evenodd" />
                </svg>
              </a>
            </div>
          </div>
        </div>
        
        {/* Bottom Copyright */}
        <div className="border-t border-gray-700 mt-8 pt-6 flex flex-col md:flex-row justify-between items-center">
          <p className="text-sm text-gray-400">
            &copy; {currentYear} S.A.G.E Project. All rights reserved.
          </p>
          <div className="mt-4 md:mt-0">
            <a href="/privacy" className="text-sm text-gray-400 hover:text-green-400 mr-4">
              Privacy Policy
            </a>
            <a href="/terms" className="text-sm text-gray-400 hover:text-green-400">
              Terms of Use
            </a>
          </div>
        </div>
      </div>
    </footer>
  );
};

export default Footer;